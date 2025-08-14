#include "LocalBrowser.hpp"
#include "Config.h"
#include "Main.hpp"
#include "Game.hpp"
#include "Gui/Host.hpp"
#include "Gui/StaticTexture.hpp"
#include "Simulation/RenderThumbnail.hpp"
#include "Common/ThreadPool.hpp"
#include "common/platform/Platform.h"
#include "client/SaveFile.h"
#include "client/SaveInfo.h"
#include "client/GameSave.h"
#include "graphics/VideoBuffer.h"

namespace Powder::Activity
{
	namespace
	{
		struct ReadFileError : public std::runtime_error // TODO: use exceptions in Platform::ReadFile
		{
			using runtime_error::runtime_error;
		};
	}

	LocalBrowser::LocalBrowser(Gui::Host &newHost) : Browser(newHost)
	{
	}

	LocalBrowser::~LocalBrowser() = default;

	void LocalBrowser::OpenItem(Item &item)
	{
		auto &localItem = static_cast<LocalItem &>(item);
		if (localItem.gameSave)
		{
			auto saveFile = std::make_unique<SaveFile>(localItem.path, false);
			saveFile->SetDisplayName(localItem.title.FromUtf8());
			saveFile->SetGameSave(std::make_unique<GameSave>(*localItem.gameSave));
			game->SetSave(std::move(saveFile), game->GetLoadWithoutPressure());
			stacks->SetHostStack(gameStack);
			Exit();
		}
	}

	void LocalBrowser::AcquireThumbnail(Item &item)
	{
		auto &localItem = static_cast<LocalItem &>(item);
		if (localItem.gameSave && !localItem.thumbnailFuture.valid())
		{
			localItem.thumbnailFuture = Simulation::RenderThumbnail(
				threadPool,
				std::make_unique<GameSave>(*localItem.gameSave),
				GetOriginalThumbnailSize(),
				RendererSettings::decorationEnabled,
				false
			);
		}
	}

	void LocalBrowser::BeginSearch()
	{
		auto pageSize = GetPageSize();
		auto pageSizeLinear = pageSize.X * pageSize.Y;
		items = ItemsLoading{};
		std::promise<ItemsData> promiseOuter;
		itemsDataFuture = promiseOuter.get_future();
		threadPool->PushWorkItem([
			promise = std::move(promiseOuter),
			pageSizeLinear,
			page = query.page,
			str = query.str
		]() mutable {
			auto dir = ByteString::Build(LOCAL_SAVE_DIR, PATH_SEP_CHAR);
			auto files = Platform::DirectorySearch(dir, str, { ".cps" });
			std::sort(files.begin(), files.end());
			auto items = std::make_shared<std::vector<std::shared_ptr<Item>>>();
			for (int32_t i = 0; i < int32_t(files.size()); ++i)
			{
				if (i >= page * pageSizeLinear && i < (page + 1) * pageSizeLinear)
				{
					auto &file = files[i];
					auto item = std::make_shared<LocalItem>();
					item->title = file.SplitFromEndBy(PATH_SEP_CHAR).After().SplitFromEndBy('.').Before();
					item->path = dir + file;
					items->push_back(item);
				}
			}
			promise.set_value({ items, int32_t(files.size()), false });
		});
	}

	void LocalBrowser::GuiManage()
	{
		auto bigButton = GetHost().GetCommonMetrics().bigButton;
		Button("delete", "Delete", bigButton); // TODO-REDO_UI-TRANSLATE
		Button("rename", "Rename", bigButton); // TODO-REDO_UI-TRANSLATE
	}

	void LocalBrowser::HandleTick()
	{
		if (itemsDataFuture.valid() && itemsDataFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			items = itemsDataFuture.get();
			for (auto &item : *std::get<ItemsData>(items).items)
			{
				auto &localItem = static_cast<LocalItem &>(*item);
				std::promise<std::unique_ptr<GameSave>> promiseOuter;
				localItem.gameSaveFuture = promiseOuter.get_future();
				threadPool->PushWorkItem([
					promise = std::move(promiseOuter),
					path = localItem.path
				]() mutable {
					try
					{
						std::vector<char> data;
						if (!Platform::ReadFile(data, path))
						{
							promise.set_exception(std::make_exception_ptr(ReadFileError("cannot access file")));
							return;
						}
						promise.set_value(std::make_unique<GameSave>(std::move(data)));
					}
					catch (const ParseException &ex)
					{
						promise.set_exception(std::current_exception());
					}
				});
			}
		}
		if (auto *itemsData = std::get_if<ItemsData>(&items))
		{
			for (auto &item : *itemsData->items)
			{
				auto &localItem = static_cast<LocalItem &>(*item);
				if (localItem.gameSaveFuture.valid() && localItem.gameSaveFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				{
					try
					{
						localItem.gameSave = localItem.gameSaveFuture.get();
					}
					catch (const ReadFileError &ex)
					{
						localItem.thumbnail = ThumbnailError{ ex.what() };
					}
					catch (const ParseException &ex)
					{
						localItem.thumbnail = ThumbnailError{ ex.what() };
					}
				}
				if (localItem.thumbnailFuture.valid() && localItem.thumbnailFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				{
					auto original = localItem.thumbnailFuture.get();
					auto small = std::make_unique<VideoBuffer>(*original);
					small->Resize(GetSmallThumbnailSize(), true);
					localItem.thumbnail = ThumbnailData{
						std::make_unique<Gui::StaticTexture>(GetHost(), false, std::move(small)),
						std::make_unique<Gui::StaticTexture>(GetHost(), false, std::move(original)),
					};
				}
			}
		}
		Browser::HandleTick();
	}

	void LocalBrowser::Gui()
	{
		SetTitle("Open local simulation"); // TODO-REDO_UI-TRANSLATE
		Browser::Gui();
	}
}
