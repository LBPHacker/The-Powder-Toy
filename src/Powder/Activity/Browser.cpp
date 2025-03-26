#include "Browser.hpp"
#include "BrowserCommon.hpp"
#include "Main.hpp"
#include "Game.hpp"
#include "SaveButton.hpp"
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
		constexpr Gui::Ticks queryDebounce = 600;
	}

	Browser::Browser(Gui::Host &newHost, ThreadPool &newThreadPool) : View(newHost), threadPool(newThreadPool)
	{
	}

	int32_t Browser::GetSelectedCount() const
	{
		int32_t count = 0;
		if (auto *itemsData = std::get_if<ItemsData>(&items))
		{
			for (auto &item : *itemsData->items)
			{
				if (item->selected)
				{
					count += 1;
				}
			}
		}
		return count;
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
		auto bottom = ScopedHPanel("bottom");
		SetSize(GetHost().GetCommonMetrics().heightToFitSize);
		SetPadding(Common{});
		SetSpacing(Common{});
		if (GetSelectedCount())
		{
			GuiManage();
			if (Button("clearSelection", "Clear selection", GetHost().GetCommonMetrics().hugeButton)) // TODO-REDO_UI-TRANSLATE
			{
				ClearSelection();
			}
		}
		else
		{
			GuiPagination();
		}
	}

	void Browser::GuiGrid()
	{
		auto gridLayers = ScopedVPanel("gridLayers");
		SetLayered(true);
		struct ZoomInfo
		{
			BrowserItem *item;
			Rect rect;
		};
		std::optional<ZoomInfo> zoomInfo;
		Rect gridRect{ { 0, 0 }, { 0, 0 } };
		{
			auto grid = ScopedVPanel("grid");
			gridRect = GetRect();
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
					GuiNoResults();
				}
				else
				{
					auto hideFirstRow = GuiQuickNav();
					auto pageSize = GetPageSize();
					for (auto y = 0; y < pageSize.Y - (hideFirstRow ? 1 : 0); ++y)
					{
						auto yPanel = ScopedHPanel(y);
						SetAlignment(Gui::Alignment::left);
						for (auto x = 0; x < pageSize.X; ++x)
						{
							auto xPanel = ScopedHPanel(x);
							auto index = y * pageSize.X + x;
							auto *item = (index < int32_t(items.size())) ? items[index].get() : nullptr;
							if (item && item->selected)
							{
								g.FillRect(GetRect().Inset(1), 0x6464AAFF_argb);
							}
							auto cell = ScopedComponent("cell"); // for sizing purposes
							auto saveButtonSize = GetSaveButtonSize();
							SetSize(saveButtonSize.X);
							SetSizeSecondary(saveButtonSize.Y);
							if (item)
							{
								auto clickable = ScopedVPanel("clickable"); // for input purposes
								SetCursor(Cursor::hand);
								SetParentFillRatio(0);
								SetParentFillRatioSecondary(0);
								SetAlignmentSecondary(Gui::Alignment::center);
								auto saveButton = ScopedVPanel("saveButton");
								{
									if (std::holds_alternative<ThumbnailLoading>(item->thumbnail))
									{
										AcquireItemThumbnail(*item);
									}
									auto [ zoomThumbnailRect, subtitleClicked ] = GuiSaveButton(*this, *item, true);
									if (subtitleClicked)
									{
										ClickItemSubtitle(*item);
									}
									if (zoomThumbnailRect)
									{
										zoomInfo = ZoomInfo{ item, *zoomThumbnailRect };
									}
									SetHandleButtons(true);
									GuiSaveButtonHandleClicks(*item);
									if (IsClicked(SDL_BUTTON_MIDDLE))
									{
										item->selected = !item->selected;
									}
									if (MaybeBeginContextmenu("context"))
									{
										GuiSaveButtonContextmenu(*item);
										if (ContextmenuButton("open", "Open")) // TODO-REDO_UI-TRANSLATE
										{
											OpenItem(*item);
										}
										if (ContextmenuButton("select", item->selected ? Gui::View::StringView("Deselect") : Gui::View::StringView("Select"))) // TODO-REDO_UI-TRANSLATE
										{
											item->selected = !item->selected;
										}
										EndContextmenu();
									}
								}
							}
						}
					}
				}
			}
			else
			{
				auto &itemsError = std::get<ItemsError>(items);
				Text("error", ByteString::Build("Search failed: ", itemsError.message)); // TODO-REDO_UI-TRANSLATE
			}
		}
		auto gridZoom = ScopedVPanel("gridZoom");
		if (zoomInfo)
		{
			GuiSaveButtonZoom(*this, gridRect, zoomInfo->rect, *zoomInfo->item);
		}
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
		auto search = ScopedHPanel("search");
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
		GuiTitle();
		auto browser = ScopedVPanel("browser");
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
		Separator("searchSeparator");
		GuiGrid();
		Separator("bottomSeparator");
		GuiBottom();
	}

	Browser::Size2 Browser::GetPageSize() const
	{
		return { 5, 4 };
	}

	void Browser::Exit()
	{
		stacks->SelectStack(gameStack);
	}

	bool Browser::Cancel()
	{
		if (GetSelectedCount())
		{
			ClearSelection();
			return true;
		}
		Exit();
		return true;
	}

	void Browser::SetQuery(Query newQuery)
	{
		firstSearchDone = true;
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
			SetQuery({});
		}
		FocusQuery();
	}

	bool Browser::GuiQuickNav()
	{
		return false;
	}

	void Browser::GuiSaveButtonContextmenu(BrowserItem &item)
	{
	}

	void Browser::GuiSaveButtonHandleClicks(BrowserItem &item)
	{
		if (IsClicked(SDL_BUTTON_LEFT))
		{
			OpenItem(item);
		}
	}

	void Browser::ClickItemSubtitle(BrowserItem &item)
	{
	}
}
