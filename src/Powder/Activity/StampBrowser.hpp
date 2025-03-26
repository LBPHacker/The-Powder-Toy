#pragma once
#include "Browser.hpp"
#include <future>

class SaveFile;
class VideoBuffer;

namespace Powder::Activity
{
	class StampBrowser : public Browser
	{
		void OpenItem(BrowserItem &item) final override;
		void AcquireItemThumbnail(BrowserItem &item) final override;
		void BeginSearch() final override;
		void GuiManage() final override;
		void GuiNoResults() final override;
		void GuiSearchLeft() final override;
		void GuiTitle() final override;
		void HandleTick() final override;

		struct StampItem : public BrowserItem
		{
			std::future<std::unique_ptr<SaveFile>> saveFileFuture;
			std::unique_ptr<SaveFile> saveFile;
			std::future<std::unique_ptr<VideoBuffer>> thumbnailFuture;
		};
		std::future<ItemsData> itemsDataFuture;

	public:
		using Browser::Browser;
		~StampBrowser();

		void Open();
	};
}
