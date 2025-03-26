#include "StampBrowser.hpp"
#include "BrowserCommon.hpp"
#include "Main.hpp"
#include "Game.hpp"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "Gui/StaticTexture.hpp"
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
		if (Button("delete", "Delete", bigButton)) // TODO-REDO_UI-TRANSLATE
		{
			PushConfirm("Delete stamps", ByteString::Build("Are you sure you want to delete ", GetSelectedCount(), " stamps?"), std::nullopt, [this, &itemsData](bool yes) { // TODO-REDO_UI-TRANSLATE
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
		if (GetSelectedCount() == 1 && Button("rename", "Rename", bigButton)) // TODO-REDO_UI-TRANSLATE
		{
			for (auto &item : *itemsData.items)
			{
				if (item->selected)
				{
					// TODO: approximate error ahead of time
					PushInput("Rename stamp", "Enter a new name for the stamp:", item->title, "[new name]", [this, &item](std::optional<std::string> newTitle) { // TODO-REDO_UI-TRANSLATE
						if (!newTitle)
						{
							return false;
						}
						if (newTitle->empty())
						{
							PushMessage("Error renaming stamp", "You have to specify the filename.", true, nullptr); // TODO-REDO_UI-TRANSLATE
							return false;
						}
						if (auto error = Client::Ref().RenameStamp(item->title, *newTitle))
						{
							PushMessage("Error renaming stamp", *error, true, nullptr); // TODO-REDO_UI-TRANSLATE
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
						stampItem.thumbnail = ThumbnailError{ ByteString("Failed to load stamp: ") + ex.what() }; // TODO-REDO_UI-TRANSLATE
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
		SetTitle("Pick stamp to place"); // TODO-REDO_UI-TRANSLATE
	}

	void StampBrowser::GuiNoResults()
	{
		Text("empty", "No stamps found", Gui::colorGray.WithAlpha(255)); // TODO-REDO_UI-TRANSLATE
	}

	void StampBrowser::GuiSearchLeft()
	{
		BeginButton("rescan", "Rescan", ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
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
