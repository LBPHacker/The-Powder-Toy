#pragma once
#include "SaveButton.hpp"
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
	public:
		struct Query
		{
			int32_t page = 0;
			std::string str;
		};
		void SetQuery(Query newQuery);

	protected:
		ThreadPool &threadPool;

		void Gui() final override;
		bool Cancel() final override;
		void HandleTick() override; // Call this at the end of overriding functions.
		void Exit() final override;

		struct ItemsLoading
		{
		};
		struct ItemsData
		{
			// wrapped in shared_ptr so it items can be replaced while iterating through the old vector
			std::shared_ptr<std::vector<std::shared_ptr<BrowserItem>>> items;
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
		virtual void OpenItem(BrowserItem &item) = 0;
		virtual void AcquireItemThumbnail(BrowserItem &item) = 0;
		virtual void ClickItemSubtitle(BrowserItem &item); // No need to call this from overriding functions.

		Gui::PaginationContext paginationContext;
		bool pageCountStale = false;
		bool CanLoadNextPage() const;
		bool CanLoadPrevPage() const;
		void LoadNextPage();
		void LoadPrevPage();

		Size2 GetPageSize() const;

		Query query;

		void FocusQuery();
		bool GetShowingFeatured() const;
		virtual void GuiPaginationLeft(); // Call this at the beginning of overriding functions if the prev button should be visible.
		virtual void GuiPaginationRight(); // Call this at the end of overriding functions if the next button should be visible.
		virtual bool GuiQuickNav(); // No need to call this from overriding functions.

		Game *game = nullptr;
		Gui::Stack *gameStack = nullptr;
		Stacks *stacks = nullptr;

		int32_t GetSelectedCount() const;
		void ClearSelection();

	private:
		virtual void BeginSearch() = 0;

		bool firstSearchDone = false;
		bool focusQuery = false;

		virtual void GuiSaveButtonContextmenu(BrowserItem &item); // No need to call this from overriding functions.
		virtual void GuiSaveButtonHandleClicks(BrowserItem &item); // No need to call this from overriding functions.
		void GuiGrid();
		void GuiBottom();

		void GuiPagination();
		virtual void GuiManage() = 0;
		virtual void GuiTitle() = 0;
		virtual void GuiNoResults() = 0;

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
		Browser(Gui::Host &newHost, ThreadPool &newThreadPool);
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

		void Open();
	};
}
