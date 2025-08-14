#include "Browser.hpp"
#include "Main.hpp"
#include "BrowserCommon.hpp"
#include "Game.hpp"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/SdlAssert.hpp"
#include "Gui/StaticTexture.hpp"
#include "Common/Log.hpp"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::Ticks queryDebounce      = 600;
		constexpr Gui::Ticks zoomThumbnailDelay = 600;
		constexpr Gui::View::Size titleHeight   =  11;
	}

	std::optional<Rect> Browser::GuiSaveButton(int32_t index, Item &item)
	{
		auto &g = GetHost();
		auto saveButtonHovered = IsHovered();
		auto colors = saveButtonHovered ? saveButtonColorsHovered : saveButtonColorsIdle;
		auto thumbnailSize = GetSmallThumbnailSize();
		SetOrder(Order::bottomToTop);
		auto deemphasizeTitle = GuiSaveButtonSubtitle(item, saveButtonHovered);
		BeginText("title", item.title, "...", TextFlags::none, deemphasizeTitle ? saveButtonColorsIdle.title : colors.title);
		SetSize(titleHeight);
		EndText();
		BeginHPanel("thumbnail");
		auto thumbnailRect = GetRect();
		SetSize(thumbnailSize.Y);
		std::optional<Rect> zoomThumbnailRectOut;
		{
			std::optional<Rect> zoomThumbnailRect;
			BeginComponent("texture");
			SetLayered(true);
			SetSize(thumbnailSize.X);
			auto r = GetRect();
			if (std::holds_alternative<ThumbnailLoading>(item.thumbnail))
			{
				Spinner("loading", g.GetCommonMetrics().smallSpinner);
				AcquireThumbnail(item);
			}
			else if (auto *thumbnailData = std::get_if<ThumbnailData>(&item.thumbnail))
			{
				g.DrawStaticTexture(Rect{ r.pos, thumbnailSize }, *thumbnailData->smallTexture, 0xFFFFFFFF_argb);
				if (IsHovered())
				{
					zoomThumbnailRect = thumbnailRect;
				}
			}
			else
			{
				auto &thumbnailError = std::get<ThumbnailError>(item.thumbnail);
				BeginText("error", ByteString::Build(Gui::iconBrokenImage, "\n", thumbnailError.message), TextFlags::multiline);
				EndText();
			}
			g.DrawRect(Rect{ r.pos, thumbnailSize }, colors.edge);
			SetAlignmentSecondary(Gui::Alignment::top);
			bool checkboxHovered = false;
			if (saveButtonHovered)
			{
				BeginVPanel("savebuttonhover");
				SetParentFillRatioSecondary(0);
				SetPadding(4, 4);
				SetAlignmentSecondary(Gui::Alignment::right);
				BeginCheckbox("selected", "", item.selected, CheckboxFlags::none);
				SetSize(12);
				SetParentFillRatioSecondary(0);
				checkboxHovered = IsHovered();
				EndCheckbox();
				EndPanel();
			}
			if (zoomThumbnailRect && !checkboxHovered)
			{
				if (!item.zoomBeganAt)
				{
					item.zoomBeganAt = g.GetLastTick();
				}
			}
			else
			{
				item.zoomBeganAt.reset();
			}
			if (item.zoomBeganAt && *item.zoomBeganAt + zoomThumbnailDelay <= g.GetLastTick())
			{
				zoomThumbnailRectOut = zoomThumbnailRect;
			}
			EndComponent();
		}
		GuiSaveButtonThumbnailRight(item, saveButtonHovered);
		EndComponent();
		SetHandleButtons(true);
		GuiHandleClicks(item);
		if (IsClicked(SDL_BUTTON_MIDDLE))
		{
			ToggleItemSelected(item);
		}
		if (MaybeBeginContextmenu("context"))
		{
			GuiSaveButtonContextmenu(item);
			if (ContextmenuButton("open", "Open")) // TODO-REDO_UI-TRANSLATE
			{
				OpenItem(item);
			}
			if (ContextmenuButton("select", item.selected ? StringView("Deselect") : StringView("Select"))) // TODO-REDO_UI-TRANSLATE
			{
				ToggleItemSelected(item);
			}
			EndContextmenu();
		}
		return zoomThumbnailRectOut;
	}

	void Browser::ToggleItemSelected(Item &item)
	{
		item.selected = !item.selected;
	}

	bool Browser::HaveSelection() const
	{
		bool haveSelection = false;
		if (auto *itemsData = std::get_if<ItemsData>(&items))
		{
			for (auto &item : *itemsData->items)
			{
				if (item->selected)
				{
					haveSelection = true;
				}
			}
		}
		return haveSelection;
	}

	void Browser::ClearSelection()
	{
		if (auto *itemsData = std::get_if<ItemsData>(&items))
		{
			for (auto &item : *itemsData->items)
			{
				item->selected = false;
			}
		}
	}

	void Browser::GuiBottom()
	{
		BeginHPanel("bottom");
		SetSize(GetHost().GetCommonMetrics().heightToFitSize);
		SetPadding(Common{});
		SetSpacing(Common{});
		if (HaveSelection())
		{
			GuiManage();
			if (Button("clearselection", "Clear selection", GetHost().GetCommonMetrics().hugeButton)) // TODO-REDO_UI-TRANSLATE
			{
				ClearSelection();
			}
		}
		else
		{
			GuiPagination();
		}
		EndPanel();
	}

	void Browser::GuiGrid()
	{
		BeginVPanel("gridlayers");
		SetLayered(true);
		struct ZoomInfo
		{
			Item *item;
			Rect rect;
		};
		std::optional<ZoomInfo> zoomInfo;
		BeginVPanel("grid");
		auto gridRect = GetRect();
		{
			auto &g = GetHost();
			if (std::holds_alternative<ItemsLoading>(items))
			{
				Spinner("loading", g.GetCommonMetrics().hugeSpinner);
			}
			else if (auto *itemsData = std::get_if<ItemsData>(&items))
			{
				auto itemsPtr = itemsData->items;
				auto &items = *itemsPtr;
				if (items.empty())
				{
					Text("empty", "\bgNo saves found"); // TODO-REDO_UI-TRANSLATE
				}
				else
				{
					auto hideFirstRow = GuiQuickNav();
					auto pageSize = GetPageSize();
					for (auto y = 0; y < pageSize.Y - (hideFirstRow ? 1 : 0); ++y)
					{
						BeginHPanel(y);
						SetAlignment(Gui::Alignment::left);
						for (auto x = 0; x < pageSize.X; ++x)
						{
							BeginHPanel(x);
							auto index = y * pageSize.X + x;
							auto *item = (index < int32_t(items.size())) ? items[index].get() : nullptr;
							if (item && item->selected)
							{
								g.FillRect(GetRect().Inset(1), 0x6464AAFF_argb);
							}
							BeginComponent("cell"); // for sizing purposes
							auto saveButtonSize = GetSaveButtonSize();
							SetSize(saveButtonSize.X);
							SetSizeSecondary(saveButtonSize.Y);
							if (item)
							{
								BeginVPanel("clickable"); // for input purposes
								SetCursor(Cursor::hand);
								SetParentFillRatio(0);
								SetParentFillRatioSecondary(0);
								SetAlignmentSecondary(Gui::Alignment::center);
								BeginVPanel("savebutton");
								auto zoomThumbnailRect = GuiSaveButton(index, *item);
								if (zoomThumbnailRect)
								{
									zoomInfo = ZoomInfo{ item, *zoomThumbnailRect };
								}
								EndPanel();
								EndPanel();
							}
							EndComponent();
							EndComponent();
						}
						EndPanel();
					}
				}
			}
			else
			{
				auto &itemsError = std::get<ItemsError>(items);
				Text("error", ByteString::Build("Search failed: ", itemsError.message)); // TODO-REDO_UI-TRANSLATE
			}
		}
		EndPanel();
		BeginVPanel("gridzoom");
		if (zoomInfo)
		{
			auto &g = GetHost();
			auto &thumbnailData = std::get<ThumbnailData>(zoomInfo->item->thumbnail);
			BeginVPanel("zoomed");
			auto zoomedRect = GetRect();
			g.FillRect(zoomedRect, 0xFF000000_argb);
			g.DrawRect(zoomedRect.Inset(1), 0xFF808080_argb);
			SetParentFillRatio(0);
			SetParentFillRatioSecondary(0);
			SetPadding(2);
			BeginComponent("thumbnail");
			auto size = GetOriginalThumbnailSize();
			SetSize(size.Y);
			SetSizeSecondary(size.X);
			auto r = GetRect();
			g.DrawStaticTexture(r, *thumbnailData.originalTexture, 0xFFFFFFFF_argb);
			EndComponent();
			Separator("separator");
			BeginVPanel("bottom");
			SetPadding(Common{});
			BeginText("title", zoomInfo->item->title, "...", TextFlags::none, saveButtonColorsIdle.title);
			SetSize(titleHeight);
			EndText();
			GuiSaveButtonSubtitle(*zoomInfo->item, false);
			EndPanel();
			{
				auto center = zoomInfo->rect.pos + zoomInfo->rect.size / 2;
				auto clampTo = gridRect;
				auto toCenter = r.Inset(-2);
				clampTo.size -= zoomedRect.size - Size2{ 1, 1 };
				auto thumbnailAt = RobustClamp(center - toCenter.size / 2, clampTo);
				ForcePosition(thumbnailAt);
			}
			EndPanel();
		}
		EndPanel();
		EndPanel();
	}

	void Browser::GuiPaginationLeft()
	{
		BeginButton("prev", ByteString::Build(Gui::iconLeft, " Prev"), ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
		SetEnabled(CanLoadPrevPage());
		SetSize(GetHost().GetCommonMetrics().bigButton);
		if (EndButton())
		{
			LoadPrevPage();
			FocusQuery();
		}
	}

	void Browser::GuiPaginationRight()
	{
		BeginButton("next", ByteString::Build("Next ", Gui::iconRight), ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
		SetEnabled(CanLoadNextPage());
		SetSize(GetHost().GetCommonMetrics().bigButton);
		if (EndButton())
		{
			LoadNextPage();
			FocusQuery();
		}
	}

	void Browser::GuiPagination()
	{
		GuiPaginationLeft();
		struct Rw
		{
			Browser &view;
			int32_t Read() { return view.query.page; }
			void Write(int32_t value) { view.QueueQuery({ value, view.query.str }); }
		};
		paginationContext.Gui("page", Rw{ *this });
		GuiPaginationRight();
	}

	void Browser::GuiSearchLeft()
	{
	}

	void Browser::GuiSearchRight()
	{
	}

	void Browser::QueueQuery(Query newQuery)
	{
		queuedQuery = { newQuery, GetHost().GetLastTick() };
	}

	void Browser::GuiSearch()
	{
		BeginHPanel("search");
		SetSize(GetHost().GetCommonMetrics().heightToFitSize);
		SetPadding(Common{});
		SetSpacing(Common{});
		GuiSearchLeft();
		BeginTextbox("query", query.str, "[search, F1 for help]", TextboxFlags::none); // TODO-REDO_UI-TRANSLATE
		if (focusQuery)
		{
			GiveInputFocus();
			TextboxSelectAll();
			focusQuery = false;
		}
		if (EndTextbox())
		{
			QueueQuery({ 0, query.str });
		}
		SetSpacing(-1);
		if (Button("clear", Gui::iconCross, Common{}))
		{
			SetQuery({});
			FocusQuery();
		}
		SetSpacing(Common{});
		GuiSearchRight();
		EndPanel();
	}

	bool Browser::CanLoadNextPage() const
	{
		return query.page < paginationContext.GetPageCount() - 1;
	}

	bool Browser::CanLoadPrevPage() const
	{
		return query.page > 0;
	}

	void Browser::LoadNextPage()
	{
		if (CanLoadNextPage())
		{
			SetQuery({ query.page + 1, query.str });
			FocusQuery();
		}
	}

	void Browser::LoadPrevPage()
	{
		if (CanLoadPrevPage())
		{
			SetQuery({ query.page - 1, query.str });
			FocusQuery();
		}
	}

	void Browser::Gui()
	{
		BeginVPanel("browser");
		SetRootRect(GetHost().GetSize().OriginRect());
		SetHandleWheel(true);
		if (auto scrollDistance = GetScrollDistance())
		{
			if (scrollDistance->Y < 0)
			{
				LoadNextPage();
			}
			if (scrollDistance->Y > 0)
			{
				LoadPrevPage();
			}
		}
		GuiSearch();
		Separator("searchseparator");
		GuiGrid();
		Separator("bottomseparator");
		GuiBottom();
		EndPanel();
	}

	Browser::Size2 Browser::GetPageSize() const
	{
		return { 5, 4 };
	}

	Browser::Size2 Browser::GetSmallThumbnailSize() const
	{
		return { 108, 68 };
	}

	Browser::Size2 Browser::GetOriginalThumbnailSize() const
	{
		return RES / 3;
	}

	Browser::Size2 Browser::GetSaveButtonSize() const
	{
		return GetSmallThumbnailSize() + Size2{ 6, 22 };
	}

	void Browser::Exit()
	{
		stacks->SetHostStack(gameStack);
	}

	bool Browser::Cancel()
	{
		if (HaveSelection())
		{
			ClearSelection();
			return true;
		}
		Exit();
		return true;
	}

	void Browser::SetQuery(Query newQuery)
	{
		pageCountStale = true;
		query = newQuery;
		paginationContext.ForceRead();
		BeginSearch();
	}

	void Browser::HandleTick()
	{
		if (auto *itemsData = std::get_if<ItemsData>(&items); itemsData && pageCountStale)
		{
			auto pageSize = GetPageSize();
			auto pageSizeLinear = pageSize.X * pageSize.Y;
			pageCountStale = false;
			paginationContext.SetPageCount(std::max(1, (itemsData->itemCountOnAllPages + pageSizeLinear - 1) / pageSizeLinear + (itemsData->includesFeatured ? 1 : 0)));
		}
		if (queuedQuery &&
		    queuedQuery->queuedAt + queryDebounce <= GetHost().GetLastTick() &&
		    !std::holds_alternative<ItemsLoading>(items))
		{
			SetQuery(queuedQuery->query);
			queuedQuery.reset();
		}
	}

	void Browser::FocusQuery()
	{
		focusQuery = true;
	}

	void Browser::Open()
	{
		if (!firstSearchDone)
		{
			firstSearchDone = true;
			SetQuery({});
		}
		FocusQuery();
	}

	bool Browser::GuiQuickNav()
	{
		return false;
	}

	bool Browser::GuiSaveButtonSubtitle(Item &item, bool saveButtonHovered)
	{
		return false;
	}

	void Browser::GuiSaveButtonThumbnailRight(Item &item, bool saveButtonHovered)
	{
	}

	void Browser::GuiSaveButtonContextmenu(Item &item)
	{
	}

	void Browser::GuiHandleClicks(Item &item)
	{
		if (IsClicked(SDL_BUTTON_LEFT))
		{
			OpenItem(item);
		}
	}
}
