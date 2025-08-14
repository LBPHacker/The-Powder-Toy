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
		void OpenItem(Item &item) final override;
		void AcquireThumbnail(Item &item) final override;
		void BeginSearch() final override;
		void GuiManage() final override;
		void Gui() final override;
		void HandleTick() final override;

		struct LocalItem : public Item
		{
			ByteString path;
			std::future<std::unique_ptr<GameSave>> gameSaveFuture;
			std::unique_ptr<GameSave> gameSave;
			std::future<std::unique_ptr<VideoBuffer>> thumbnailFuture;
		};
		std::future<ItemsData> itemsDataFuture;

	public:
		LocalBrowser(Gui::Host &newHost);
		~LocalBrowser();
	};
}
