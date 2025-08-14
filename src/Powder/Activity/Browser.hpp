#pragma once
#include "Gui/View.hpp"

namespace Powder::Gui
{
	class StaticTexture;
}

namespace Powder::Activity
{
	class Game;

	class Browser : public Gui::View
	{
		bool HandleEvent(const SDL_Event &event) final override;
		void Gui() final override;
		void Exit();

	protected:
		void HandleTick() override; // Call this at the end of overriding functions.

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
			std::shared_ptr<ThumbnailData>,
			ThumbnailError
		>;

		struct Item
		{
			Thumbnail thumbnail;
			ByteString title;
			struct OnlineInfo
			{
				ByteString author;
				int32_t scoreUp;
				int32_t scoreDown;
				bool published;
			};
			std::optional<OnlineInfo> onlineInfo;
		};
		struct ItemsLoading
		{
		};
		struct ItemsData
		{
			std::vector<std::shared_ptr<Item>> items;
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

		int32_t pageCount = 1;
		bool pageCountStale = false;
		bool CanLoadNextPage() const;
		bool CanLoadPrevPage() const;
		void LoadNextPage();
		void LoadPrevPage();

		Size2 GetPageSize() const;
		Size2 GetThumbnailSize() const;
		Size2 GetSaveButtonSize() const;
		int32_t GetPageDigitCount() const;

		struct Query
		{
			int32_t page = 0;
			std::string str;
		};
		Query query;
		std::string pageString;
		std::optional<int32_t> pageStringParsed;
		bool pageTextboxHasFocus = false;
		void UpdatePageString();
		void ResetPageString();
		void BeginSearchInternal(Query newQuery);

		void FocusQuery();
		bool GetShowingFeatured() const;
		virtual void GuiPaginationLeft(); // Call this at the beginning of overriding functions if the prev button should be visible.
		virtual void GuiPaginationRight(); // Call this at the end of overriding functions if the next button should be visible.
		virtual bool GuiQuickNav(); // No need to call this from overriding functions.

	private:
		virtual void BeginSearch() = 0;

		Game *game = nullptr;
		std::shared_ptr<Gui::Stack> gameStack;
		bool firstSearchDone = false;
		bool focusQuery = false;

		std::optional<int32_t> saveButtonAuthorHovered;
		std::optional<int32_t> nextSaveButtonAuthorHovered;
		void GuiVoteBars(Item &item, bool saveButtonHovered);
		void GuiSaveButton(int32_t index, Item &item);
		void GuiGrid();

		void GuiPagination();

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

		void SetGameStack(std::shared_ptr<Gui::Stack> newGameStack)
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
