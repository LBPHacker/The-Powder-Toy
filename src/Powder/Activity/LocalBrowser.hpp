#pragma once
#include "Browser.hpp"
#include <future>

class SaveFile;
class GameSave;
class VideoBuffer;

namespace Powder::Activity
{
	class LocalBrowser : public Browser
	{
		void OpenItem(BrowserItem &item) final override;
		void AcquireItemThumbnail(BrowserItem &item) final override;
		void BeginSearch() final override;
		void GuiManage() final override;
		void GuiNoResults() final override;
		void GuiTitle() final override;
		void HandleTick() final override;

		struct LocalItem : public BrowserItem
		{
			ByteString path;
			std::future<std::unique_ptr<GameSave>> gameSaveFuture;
			std::unique_ptr<GameSave> gameSave;
			std::future<std::unique_ptr<VideoBuffer>> thumbnailFuture;
		};
		std::future<ItemsData> itemsDataFuture;

	public:
		using Browser::Browser;
		~LocalBrowser();
	};
}
