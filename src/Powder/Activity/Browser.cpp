#include "Browser.hpp"
#include "Game.hpp"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/StaticTexture.hpp"
#include "Common/Log.hpp"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::Ticks queryDebounce = 600;

		constexpr Rgba8 voteUpBackground   = 0xFF006B0A_argb;
		constexpr Rgba8 voteDownBackground = 0xFF6B0A00_argb;
		constexpr Rgba8 voteUpBar          = 0xFF39BB39_argb;
		constexpr Rgba8 voteDownBar        = 0xFFBB3939_argb;

		struct SaveButtonColors
		{
			Rgba8 edge;
			Rgba8 title;
			Rgba8 author;
		};
		constexpr SaveButtonColors saveButtonColorsIdle = {
			0xFFB4B4B4_argb,
			0xFFB4B4B4_argb,
			0xFF6482A0_argb,
		};
		constexpr SaveButtonColors saveButtonColorsHovered = {
			0xFFD2E6FF_argb,
			0xFFFFFFFF_argb,
			0xFFC8E6FF_argb,
		};

		std::pair<int32_t, int32_t> ScaleVoteBars(int32_t maxSizeUp, int32_t maxSizeDown, int32_t scoreUp, int32_t scoreDown)
		{
			constexpr int32_t barFillThreshold = 8;
			int32_t sizeUp = 0;
			int32_t sizeDown = 0;
			auto scaleMax = std::max(scoreUp, scoreDown);
			if (scaleMax)
			{
				if (scaleMax < barFillThreshold)
				{
					scaleMax *= barFillThreshold - scaleMax;
				}
				sizeUp   = scoreUp   * maxSizeUp   / scaleMax;
				sizeDown = scoreDown * maxSizeDown / scaleMax;
			}
			return { sizeUp, sizeDown };
		}
	}

	void Browser::GuiVoteBars(Item &item, bool saveButtonHovered)
	{
		auto &g = GetHost();
		auto colors = saveButtonHovered ? saveButtonColorsHovered : saveButtonColorsIdle;
		BeginComponent("votebars");
		SetSize(6);
		auto r = GetRect();
		r.pos.X -= 1;
		r.size.X += 1;
		g.DrawRect(r, colors.edge);
		r = r.Inset(1);
		auto rUp = r;
		rUp.size.Y /= 2;
		auto rDown = r;
		rDown.pos.Y += rUp.size.Y;
		rDown.size.Y -= rUp.size.Y;
		g.FillRect(rUp, voteUpBackground);
		g.FillRect(rDown, voteDownBackground);
		rUp = rUp.Inset(1);
		rDown = rDown.Inset(1);
		rUp.size.Y += 1;
		rDown.pos.Y -= 1;
		rDown.size.Y += 1;
		auto [ sizeUp, sizeDown ] = ScaleVoteBars(rUp.size.Y, rDown.size.Y, item.onlineInfo->scoreUp, item.onlineInfo->scoreDown);
		rDown.size.Y = sizeDown;
		rUp.pos.Y += rUp.size.Y - sizeUp;
		rUp.size.Y = sizeUp;
		g.FillRect(rUp, voteUpBar);
		g.FillRect(rDown, voteDownBar);
		EndComponent();
	}

	void Browser::GuiSaveButton(int32_t index, Item &item)
	{
		auto &g = GetHost();
		auto hovered = IsHovered();
		auto colors = hovered ? saveButtonColorsHovered : saveButtonColorsIdle;
		auto thumbnailSize = GetThumbnailSize();
		BeginHPanel("thumbnail");
		SetSize(thumbnailSize.Y);
		{
			BeginComponent("texture");
			SetSize(thumbnailSize.X);
			auto r = GetRect();
			if (std::holds_alternative<ThumbnailLoading>(item.thumbnail))
			{
				Text("loading", "Loading..."); // TODO-REDO_UI-TRANSLATE
			}
			else if (auto *thumbnailDataPtr = std::get_if<std::shared_ptr<ThumbnailData>>(&item.thumbnail))
			{
				auto &thumbnailData = **thumbnailDataPtr;
				g.DrawStaticTexture(Rect{ r.pos, thumbnailSize }, *thumbnailData.texture, 0xFFFFFFFF_argb);
			}
			else
			{
				auto &thumbnailError = std::get<ThumbnailError>(item.thumbnail);
				BeginText("error", ByteString::Build("\uE06E\n", thumbnailError.message), Gui::View::TextFlags::multiline);
				EndText();
			}
			g.DrawRect(Rect{ r.pos, thumbnailSize }, colors.edge);
			EndComponent();
		}
		if (item.onlineInfo)
		{
			GuiVoteBars(item, hovered);
		}
		EndComponent();
		auto authorHovered = saveButtonAuthorHovered && *saveButtonAuthorHovered == index;
		BeginText("title", item.title, Gui::View::TextFlags::none, authorHovered ? saveButtonColorsIdle.title : colors.title);
		SetSize(11);
		EndText();
		if (item.onlineInfo)
		{
			BeginText("author", item.onlineInfo->author, Gui::View::TextFlags::none, colors.author);
			SetSize(11);
			if (IsHovered())
			{
				nextSaveButtonAuthorHovered = index;
				if (!authorHovered)
				{
					RequestRepeatFrame();
				}
			}
			EndText();
		}
	}

	void Browser::GuiGrid()
	{
		BeginVPanel("grid");
		{
			if (std::holds_alternative<ItemsLoading>(items))
			{
				Text("loading", "Loading..."); // TODO-REDO_UI-TRANSLATE
			}
			else if (auto *itemsData = std::get_if<ItemsData>(&items))
			{
				auto &items = itemsData->items;
				if (items.empty())
				{
					Text("empty", "No saves found"); // TODO-REDO_UI-TRANSLATE
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
							BeginComponent("cell"); // for sizing purposes
							auto saveButtonSize = GetSaveButtonSize();
							SetSize(saveButtonSize.X);
							SetSizeSecondary(saveButtonSize.Y);
							auto index = y * pageSize.X + x;
							if (index < int32_t(items.size()))
							{
								BeginVPanel("clickable"); // for input purposes
								SetParentFillRatio(0);
								SetParentFillRatioSecondary(0);
								SetAlignmentSecondary(Gui::Alignment::center);
								GuiSaveButton(index, *items[index]);
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
	}

	void Browser::GuiPaginationLeft()
	{
		BeginButton("prev", ByteString::Build(Gui::iconLeft, " Prev"), ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
		SetEnabled(CanLoadPrevPage());
		SetSize(70);
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
		SetSize(70);
		if (EndButton())
		{
			LoadNextPage();
			FocusQuery();
		}
	}

	void Browser::UpdatePageString()
	{
		pageStringParsed.reset();
		try
		{
			auto newPage = ByteString(pageString).ToNumber<int32_t>() - 1;
			if (newPage >= 0 && newPage < pageCount)
			{
				pageStringParsed = newPage;
			}
		}
		catch (const std::runtime_error &)
		{
		}
	}

	void Browser::ResetPageString()
	{
		pageString = ByteString::Build(query.page + 1);
		UpdatePageString();
	}

	void Browser::GuiPagination()
	{
		BeginHPanel("pagination");
		SetSize(25);
		SetPadding(4);
		SetSpacing(4);
		GuiPaginationLeft();
		BeginHPanel("page");
		SetSpacing(5);
		BeginText("before", "Page", TextFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
		SetParentFillRatio(0);
		EndText();
		BeginTextbox("input", pageString, TextboxFlags::none, (pageStringParsed ? Gui::colorWhite : Gui::colorRed).WithAlpha(255));
		SetSize(GetPageDigitCount() * 6 + 10);
		SetTextAlignment(Gui::Alignment::center, Gui::Alignment::center);
		{
			auto newHasFocus = HasInputFocus();
			if (pageTextboxHasFocus && !newHasFocus && !pageStringParsed)
			{
				ResetPageString();
			}
			pageTextboxHasFocus = newHasFocus;
		}
		if (EndTextbox())
		{
			UpdatePageString();
			if (pageStringParsed)
			{
				QueueQuery({ *pageStringParsed, query.str });
			}
		}
		BeginText("after", ByteString::Build("of ", pageCount), TextFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
		SetParentFillRatio(0);
		EndText();
		EndComponent();
		GuiPaginationRight();
		EndPanel();
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
		SetSize(25);
		SetPadding(4);
		SetSpacing(4);
		GuiSearchLeft();
		BeginTextbox("query", query.str, TextboxFlags::none);
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
		if (Button(Gui::iconCross, 17))
		{
			BeginSearchInternal({});
			FocusQuery();
		}
		SetSpacing(4);
		GuiSearchRight();
		EndPanel();
	}

	bool Browser::CanLoadNextPage() const
	{
		return query.page < pageCount - 1;
	}

	bool Browser::CanLoadPrevPage() const
	{
		return query.page > 0;
	}

	void Browser::LoadNextPage()
	{
		if (CanLoadNextPage())
		{
			BeginSearchInternal({ query.page + 1, query.str });
			FocusQuery();
		}
	}

	void Browser::LoadPrevPage()
	{
		if (CanLoadPrevPage())
		{
			BeginSearchInternal({ query.page - 1, query.str });
			FocusQuery();
		}
	}

	void Browser::Gui()
	{
		saveButtonAuthorHovered = nextSaveButtonAuthorHovered;
		nextSaveButtonAuthorHovered.reset();
		SetRootRect(GetHost().GetSize().OriginRect());
		BeginVPanel("browser");
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
		GuiGrid();
		GuiPagination();
		EndPanel();
	}

	Browser::Size2 Browser::GetPageSize() const
	{
		return { 5, 4 };
	}

	Browser::Size2 Browser::GetThumbnailSize() const
	{
		return { 108, 68 };
	}

	Browser::Size2 Browser::GetSaveButtonSize() const
	{
		return GetThumbnailSize() + Size2{ 6, 22 };
	}

	int32_t Browser::GetPageDigitCount() const
	{
		auto probe = pageCount;
		auto digitCount = 0;
		while (probe)
		{
			probe /= 10;
			digitCount += 1;
		}
		return digitCount;
	}

	void Browser::Exit()
	{
		GetHost().SetStack(gameStack);
	}

	bool Browser::HandleEvent(const SDL_Event &event)
	{
		auto handledByView = View::HandleEvent(event);
		if (ClassifyExitEvent(event) == Gui::View::ExitEventType::exit)
		{
			Exit();
			return true;
		}
		if (MayBeHandledExclusively(event) && handledByView)
		{
			return true;
		}
		return false;
	}

	void Browser::BeginSearchInternal(Query newQuery)
	{
		pageCountStale = true;
		query = newQuery;
		if (!pageTextboxHasFocus)
		{
			ResetPageString();
		}
		BeginSearch();
	}

	void Browser::HandleTick()
	{
		if (auto *itemsData = std::get_if<ItemsData>(&items); itemsData && pageCountStale)
		{
			auto pageSize = GetPageSize();
			auto pageSizeLinear = pageSize.X * pageSize.Y;
			pageCount = std::max(1, (itemsData->itemCountOnAllPages + pageSizeLinear - 1) / pageSizeLinear + (itemsData->includesFeatured ? 1 : 0));
			UpdatePageString();
			pageCountStale = false;
		}
		if (queuedQuery &&
		    queuedQuery->queuedAt + queryDebounce <= GetHost().GetLastTick() &&
		    !std::holds_alternative<ItemsLoading>(items))
		{
			BeginSearchInternal(queuedQuery->query);
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
			BeginSearchInternal({});
		}
		FocusQuery();
	}

	bool Browser::GuiQuickNav()
	{
		return false;
	}
}
