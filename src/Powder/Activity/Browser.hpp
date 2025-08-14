#pragma once
#include "Gui/ComplexInput.hpp"
#include "Gui/View.hpp"

namespace Powder
{
	class ThreadPool;
	class Stacks;
}

namespace Powder::Gui
{
	class StaticTexture;
}

namespace Powder::Activity
{
	class Game;

	class Browser : public Gui::View
	{
		void Gui() final override;
		bool Cancel() final override;

	protected:
		void HandleTick() override; // Call this at the end of overriding functions.
		void Exit();

		struct ThumbnailLoading
		{
		};
		struct ThumbnailData
		{
			std::unique_ptr<Gui::StaticTexture> texture;
		};
		struct ThumbnailError
		{
			ByteString message;
		};
		using Thumbnail = std::variant<
			ThumbnailLoading,
			ThumbnailData,
			ThumbnailError
		>;

		struct Item
		{
			Thumbnail thumbnail;
			ByteString title;
			bool selected = false;
		};
		struct ItemsLoading
		{
		};
		struct ItemsData
		{
			// wrapped in shared_ptr so it items can be replaced while iterating through the old vector
			std::shared_ptr<std::vector<std::shared_ptr<Item>>> items;
			int32_t itemCountOnAllPages;
			bool includesFeatured;
		};
		struct ItemsError
		{
			ByteString message;
		};
		using Items = std::variant<
			ItemsLoading,
			ItemsData,
			ItemsError
		>;
		Items items;
		virtual void OpenItem(Item &item) = 0;
		virtual void AcquireThumbnail(Item &item) = 0;
		void ToggleItemSelected(Item &item);

		Gui::PaginationContext paginationContext;
		bool pageCountStale = false;
		bool CanLoadNextPage() const;
		bool CanLoadPrevPage() const;
		void LoadNextPage();
		void LoadPrevPage();

		Size2 GetPageSize() const;
		Size2 GetThumbnailSize() const;
		Size2 GetSaveButtonSize() const;

		struct Query
		{
			int32_t page = 0;
			std::string str;
		};
		Query query;

		void SetQuery(Query newQuery);

		void FocusQuery();
		bool GetShowingFeatured() const;
		virtual void GuiPaginationLeft(); // Call this at the beginning of overriding functions if the prev button should be visible.
		virtual void GuiPaginationRight(); // Call this at the end of overriding functions if the next button should be visible.
		virtual bool GuiQuickNav(); // No need to call this from overriding functions.

		Game *game = nullptr;
		Gui::Stack *gameStack = nullptr;
		ThreadPool *threadPool = nullptr;
		Stacks *stacks = nullptr;

	private:
		virtual void BeginSearch() = 0;

		bool firstSearchDone = false;
		bool focusQuery = false;

		void GuiSaveButton(int32_t index, Item &item);
		virtual bool GuiSaveButtonSubtitle(int32_t index, Item &item, bool saveButtonHovered); // No need to call this from overriding functions.
		virtual void GuiSaveButtonThumbnailRight(int32_t index, Item &item, bool saveButtonHovered); // No need to call this from overriding functions.
		virtual void GuiSaveButtonContextmenu(int32_t index, Item &item); // No need to call this from overriding functions.
		virtual void GuiHandleClicks(Item &item); // No need to call this from overriding functions.
		void GuiGrid();
		void GuiBottom();

		bool HaveSelection() const;
		void ClearSelection();

		void GuiPagination();
		virtual void GuiManage() = 0;

		struct QueuedQuery
		{
			Query query;
			Gui::Ticks queuedAt;
		};
		std::optional<QueuedQuery> queuedQuery;
		void QueueQuery(Query newQuery);
		void GuiSearch();
		virtual void GuiSearchLeft(); // No need to call this from overriding functions.
		virtual void GuiSearchRight(); // No need to call this from overriding functions.

	public:
		using View::View;
		virtual ~Browser() = default;

		void SetStacks(Stacks *newStacks)
		{
			stacks = newStacks;
		}

		void SetGameStack(Gui::Stack *newGameStack)
		{
			gameStack = newGameStack;
		}

		void SetGame(Game *newGame)
		{
			game = newGame;
		}

		void SetThreadPool(ThreadPool *newThreadPool)
		{
			threadPool = newThreadPool;
		}

		void Open();
	};
}
