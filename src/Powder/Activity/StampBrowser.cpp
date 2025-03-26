#include "StampBrowser.hpp"
#include "BrowserCommon.hpp"
#include "Main.hpp"
#include "Game.hpp"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "Gui/StaticTexture.hpp"
#include "Gui/ViewUtil.hpp"
#include "Simulation/RenderThumbnail.hpp"
#include "Common/ThreadPool.hpp"
#include "client/Client.h"
#include "client/SaveFile.h"
#include "client/SaveInfo.h"
#include "client/GameSave.h"
#include "graphics/VideoBuffer.h"

namespace Powder::Activity
{
	namespace
	{
		struct ReadFileError : public std::runtime_error // TODO: use exceptions in Client::GetStamp
		{
			using runtime_error::runtime_error;
		};
	}

	StampBrowser::~StampBrowser() = default;

	void StampBrowser::OpenItem(BrowserItem &item)
	{
		auto &stampItem = static_cast<StampItem &>(item);
		if (stampItem.saveFile)
		{
			Client::Ref().MoveStampToFront(stampItem.title);
			game->BeginPaste(stampItem.saveFile->TakeGameSave());
			stacks->SelectStack(gameStack);
			Exit();
		}
	}

	void StampBrowser::AcquireItemThumbnail(BrowserItem &item)
	{
		auto &stampItem = static_cast<StampItem &>(item);
		if (stampItem.saveFile && !stampItem.thumbnailFuture.valid())
		{
			stampItem.thumbnailFuture = Simulation::RenderThumbnail(
				threadPool,
				std::make_unique<GameSave>(*stampItem.saveFile->GetGameSave()),
				GetOriginalThumbnailSize(),
				RendererSettings::decorationEnabled, // TODO-REDO_UI-FUTURE: decide if this is appropriate for stamps
				false
			);
		}
	}

	void StampBrowser::BeginSearch()
	{
		auto pageSize = GetPageSize();
		auto pageSizeLinear = pageSize.X * pageSize.Y;
		auto stamps = Client::Ref().GetStamps();
		auto newItems = std::make_shared<std::vector<std::shared_ptr<BrowserItem>>>();
		if (!query.str.empty())
		{
			auto queryLower = ByteString(query.str).ToLower();
			std::erase_if(stamps, [&queryLower](auto &stamp) {
				auto titleLower = stamp.ToLower();
				return titleLower.find(queryLower) == titleLower.npos;
			});
		}
		for (int32_t i = 0; i < int32_t(stamps.size()); ++i)
		{
			if (i >= query.page * pageSizeLinear && i < (query.page + 1) * pageSizeLinear)
			{
				auto &stamp = stamps[i];
				auto item = std::make_shared<StampItem>();
				item->title = stamp;
				std::promise<std::unique_ptr<SaveFile>> promise;
				item->saveFileFuture = promise.get_future();
				threadPool.PushWorkItem([
					promiseInner = std::move(promise),
					stamp
				]() mutable {
					auto saveFile = Client::Ref().GetStamp(stamp);
					if (!saveFile)
					{
						promiseInner.set_exception(std::make_exception_ptr(ReadFileError("cannot access file")));
						return;
					}
					promiseInner.set_value(std::move(saveFile));
				});
				newItems->push_back(item);
			}
		}
		items = ItemsData{ newItems, int32_t(stamps.size()), false };
	}

	void StampBrowser::GuiManage()
	{
		auto &itemsData = std::get<ItemsData>(items);
		auto bigButton = GetHost().GetCommonMetrics().bigButton;
		if (Button("delete", "DEFAULT_LANG_STAMPDELETE"_St, bigButton))
		{
			PushConfirm("DEFAULT_LANG_STAMPDELETE_TITLE"_St(), "DEFAULT_LANG_STAMPDELETE_CONTENT"_St(GetSelectedCount()), std::nullopt, [this, &itemsData](bool yes) {
				if (!yes)
				{
					return;
				}
				for (auto &item : *itemsData.items)
				{
					if (item->selected)
					{
						Client::Ref().DeleteStamp(item->title);
					}
				}
				BeginSearch();
			});
		}
		if (GetSelectedCount() == 1 && Button("rename", "DEFAULT_LANG_STAMPRENAME"_St, bigButton))
		{
			for (auto &item : *itemsData.items)
			{
				if (item->selected)
				{
					// TODO: approximate error ahead of time
					PushInput("DEFAULT_LANG_STAMPRENAME_TITLE"_St(), "DEFAULT_LANG_STAMPRENAME_DESC"_St(), item->title, BuildString("[", "DEFAULT_LANG_STAMPRENAME_NEWNAME"_St, "]"), [this, &item](std::optional<std::string> newTitle) {
						if (!newTitle)
						{
							return false;
						}
						if (newTitle->empty())
						{
							PushMessage("DEFAULT_LANG_STAMPRENAME_ERROR"_St(), "DEFAULT_LANG_STAMPRENAME_EMPTYNAME"_St(), true, nullptr);
							return false;
						}
						if (auto error = Client::Ref().RenameStamp(item->title, *newTitle))
						{
							PushMessage("DEFAULT_LANG_STAMPRENAME_ERROR"_St(), *error, true, nullptr);
							return false;
						}
						BeginSearch();
						return true;
					});
				}
			}
		}
	}

	void StampBrowser::HandleTick()
	{
		if (auto *itemsData = std::get_if<ItemsData>(&items))
		{
			for (auto &item : *itemsData->items)
			{
				auto &stampItem = static_cast<StampItem &>(*item);
				if (stampItem.saveFileFuture.valid() && stampItem.saveFileFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				{
					try
					{
						auto saveFile = stampItem.saveFileFuture.get();
						if (!saveFile->LazyGetGameSave())
						{
							throw ReadFileError(saveFile->GetError().ToUtf8());
						}
						stampItem.saveFile = std::move(saveFile);
					}
					catch (const ReadFileError &ex)
					{
						stampItem.thumbnail = ThumbnailError{ "DEFAULT_LANG_STAMPBROWSER_LOADERROR"_St(ex.what()) };
					}
				}
				if (stampItem.thumbnailFuture.valid() && stampItem.thumbnailFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				{
					auto original = stampItem.thumbnailFuture.get();
					// TODO: make stamp-like
					auto small = std::make_unique<VideoBuffer>(*original);
					original->ResizeToFit(GetOriginalThumbnailSize(), true);
					small->ResizeToFit(GetSmallThumbnailSize(), true);
					original->XorDottedRect(original->Size().OriginRect());
					small->XorDottedRect(small->Size().OriginRect());
					stampItem.thumbnail = ThumbnailData{
						std::make_unique<Gui::StaticTexture>(GetHost(), false, std::move(original)),
						std::make_unique<Gui::StaticTexture>(GetHost(), false, std::move(small)),
					};
				}
			}
		}
		Browser::HandleTick();
	}

	void StampBrowser::GuiTitle()
	{
		SetTitle("DEFAULT_LANG_STAMPBROWSER_TITLE"_St());
	}

	void StampBrowser::GuiNoResults()
	{
		Text("empty", "DEFAULT_LANG_STAMPBROWSER_NOSTAMPS"_St, Gui::colorGray.WithAlpha(255));
	}

	void StampBrowser::GuiSearchLeft()
	{
		BeginButton("rescan", "DEFAULT_LANG_STAMPBROWSER_RESCAN"_St, ButtonFlags::none);
		SetSize(GetHost().GetCommonMetrics().bigButton);
		if (EndButton())
		{
			Client::Ref().RescanStamps();
			SetQuery({});
		}
	}

	void StampBrowser::Open()
	{
		SetQuery({});
		FocusQuery();
	}
}
