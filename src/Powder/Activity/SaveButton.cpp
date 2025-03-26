#include "SaveButton.hpp"
#include "BrowserCommon.hpp"
#include "Votebars.hpp"
#include "Gui/Icons.hpp"
#include "Gui/Host.hpp"
#include "Gui/SdlAssert.hpp"
#include "Gui/StaticTexture.hpp"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::Ticks zoomThumbnailDelay = 600;
		constexpr Gui::View::Size titleHeight   =  12;
		constexpr Gui::View::Size voteBarsWidth =   6;

		void GuiSaveButtonRight(Gui::View &view, bool saveButtonHovered, BrowserItem::OnlineInfo &onlineInfo)
		{
			auto &g = view.GetHost();
			auto colors = saveButtonHovered ? saveButtonColorsHovered : saveButtonColorsIdle;
			{
				auto votebars = view.ScopedComponent("votebars");
				view.SetSize(voteBarsWidth);
				auto r = view.GetRect();
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
				auto [ sizeUp, sizeDown ] = ScaleVoteBars(rUp.size.Y, rDown.size.Y, onlineInfo.votesUp, onlineInfo.votesDown);
				rDown.size.Y = sizeDown;
				rUp.pos.Y += rUp.size.Y - sizeUp;
				rUp.size.Y = sizeUp;
				g.FillRect(rUp, voteUpBar);
				g.FillRect(rDown, voteDownBar);
			}
			{
				// at this point Host's clipRect is the entire parent component
				auto r = view.GetRect();
				auto score = onlineInfo.votesUp - onlineInfo.votesDown;
				auto scoreString = ByteString::Build(score);
				std::string bodyString;
				std::string numberString;
				std::string outlineString;
				int32_t width = 0;
				auto appendBody = [&](int32_t ch) {
					width += g.CharWidth(ch);
					Utf8Append(bodyString, ch);
				};
				appendBody(Gui::iconBubbleInitialBody);
				Utf8Append(outlineString, Gui::iconBubbleInitialOutline);
				for (int32_t i = 0; i < int32_t(scoreString.size()); ++i)
				{
					auto ch = scoreString[i];
					appendBody(i == 0 ? Gui::iconBubbleMedialFirstBody : Gui::iconBubbleMedialRestBody);
					Utf8Append(outlineString, i == 0 ? Gui::iconBubbleMedialFirstOutline : Gui::iconBubbleMedialRestOutline);
					Utf8Append(numberString, ch == '-' ? ch : (ch - '0' + Gui::iconBubble0));
				}
				appendBody(Gui::iconBubbleFinalBody);
				Utf8Append(outlineString, Gui::iconBubbleFinalOutline);
				auto p = r.BottomRight() - Gui::View::Pos2{ width + voteBarsWidth + 2, 12 };
				g.DrawText(p                          , bodyString, score >= 1 ? voteUpBackground : voteDownBackground);
				g.DrawText(p + Gui::View::Pos2{ 3, 0 }, numberString, 0xFFFFFFFF_argb);
				g.DrawText(p                          , outlineString, 0xFFFFFFFF_argb);
				if (!onlineInfo.published)
				{
					g.DrawText(r.TopLeft() + Gui::View::Pos2{ 2, r.size.Y - 11 }, Gui::iconLockOutline, 0xFFFFFFFF_argb);
				}
			}
		}
	}

	std::pair<std::optional<Rect>, bool> GuiSaveButton(Gui::View &view, BrowserItem &item, bool selectable)
	{
		auto &g = view.GetHost();
		auto saveButtonHovered = view.IsHovered();
		auto colors = saveButtonHovered ? saveButtonColorsHovered : saveButtonColorsIdle;
		auto thumbnailSize = GetSmallThumbnailSize();
		view.SetOrder(Gui::View::Order::bottomToTop);
		bool subtitleClicked = false;
		bool deemphasizeTitle = false;
		if (item.onlineInfo)
		{
			auto colors = saveButtonHovered ? saveButtonColorsHovered : saveButtonColorsIdle;
			view.BeginText("author", item.onlineInfo->author, "...", Gui::View::TextFlags::none, colors.author);
			view.SetTextPadding(0, 0, 3, 2);
			view.SetHandleButtons(true);
			view.SetSize(titleHeight);
			deemphasizeTitle = view.IsHovered();
			if (view.IsClicked(SDL_BUTTON_LEFT))
			{
				subtitleClicked = true;
			}
			if (selectable && view.IsClicked(SDL_BUTTON_MIDDLE))
			{
				// Forward middle click because our SetHandleButtons(true) prevents it from being detected by the parent savebutton.
				item.selected = !item.selected;
			}
			view.EndText();
		}
		view.BeginText("title", item.title, "...", Gui::View::TextFlags::none, deemphasizeTitle ? saveButtonColorsIdle.title : colors.title);
		view.SetTextPadding(0, 0, 3, 2);
		view.SetSize(titleHeight);
		view.EndText();
		auto thumbnailPanel = view.ScopedHPanel("thumbnail");
		auto thumbnailRect = view.GetRect();
		view.SetSize(thumbnailSize.Y);
		std::optional<Rect> zoomThumbnailRectOut;
		{
			std::optional<Rect> zoomThumbnailRect;
			auto textureComponent = view.ScopedComponent("texture");
			view.SetLayered(true);
			view.SetSize(thumbnailSize.X);
			auto r = view.GetRect();
			if (std::holds_alternative<ThumbnailLoading>(item.thumbnail))
			{
				view.Spinner("loading", g.GetCommonMetrics().smallSpinner);
			}
			else if (auto *thumbnailData = std::get_if<ThumbnailData>(&item.thumbnail))
			{
				auto to = Rect{ r.pos, thumbnailData->smallTexture->GetSize() };
				to.pos += (thumbnailSize - to.size) / 2;
				g.DrawStaticTexture(to, *thumbnailData->smallTexture, 0xFFFFFFFF_argb);
				if (view.IsHovered())
				{
					zoomThumbnailRect = thumbnailRect;
				}
			}
			else
			{
				auto &thumbnailError = std::get<ThumbnailError>(item.thumbnail);
				view.BeginText("error", ByteString::Build(Gui::iconBrokenImage, "\n", thumbnailError.message), Gui::View::TextFlags::multiline);
				view.EndText();
			}
			g.DrawRect(Rect{ r.pos, thumbnailSize }, colors.edge);
			view.SetAlignmentSecondary(Gui::Alignment::top);
			bool checkboxHovered = false;
			if (selectable && saveButtonHovered)
			{
				auto saveButtonHover = view.ScopedVPanel("saveButtonHover");
				view.SetParentFillRatioSecondary(0);
				view.SetPadding(4, 4);
				view.SetAlignmentSecondary(Gui::Alignment::right);
				view.BeginCheckbox("selected", "", item.selected, Gui::View::CheckboxFlags::none);
				view.SetSize(13);
				view.SetParentFillRatioSecondary(0);
				checkboxHovered = view.IsHovered();
				view.EndCheckbox();
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
		}
		if (item.onlineInfo)
		{
			GuiSaveButtonRight(view, saveButtonHovered, *item.onlineInfo);
		}
		return { zoomThumbnailRectOut, subtitleClicked };
	}

	void GuiSaveButtonZoom(Gui::View &view, Rect gridRect, Rect zoomRect, BrowserItem &item)
	{
		auto &g = view.GetHost();
		auto &thumbnailData = std::get<ThumbnailData>(item.thumbnail);
		auto zoomed = view.ScopedVPanel("zoomed");
		auto zoomedRect = view.GetRect();
		g.FillRect(zoomedRect, 0xFF000000_argb);
		g.DrawRect(zoomedRect.Inset(1), 0xFF808080_argb);
		view.SetParentFillRatio(0);
		view.SetParentFillRatioSecondary(0);
		view.SetPadding(2);
		Rect r{ { 0, 0 }, { 0, 0 } };
		{
			auto thumbnailComponent = view.ScopedComponent("thumbnail");
			auto size = GetOriginalThumbnailSize();
			view.SetSize(size.Y);
			view.SetSizeSecondary(size.X);
			r = view.GetRect();
			auto to = Rect{ r.pos, thumbnailData.originalTexture->GetSize() };
			to.pos += (r.size - to.size) / 2;
			g.DrawStaticTexture(to, *thumbnailData.originalTexture, 0xFFFFFFFF_argb);
		}
		view.Separator("separator");
		{
			auto bottom = view.ScopedVPanel("bottom");
			view.SetPadding(Gui::View::Common{});
			view.BeginText("title", item.title, "...", Gui::View::TextFlags::none, saveButtonColorsIdle.title);
			view.SetTextPadding(0, 0, 2, 2);
			view.SetSize(titleHeight - 1);
			view.EndText();
			if (item.onlineInfo)
			{
				view.BeginText("author", item.onlineInfo->author, "...", Gui::View::TextFlags::none, saveButtonColorsIdle.author);
				view.SetTextPadding(0, 0, 3, 2);
				view.SetSize(titleHeight);
				view.EndText();
			}
		}
		{
			auto center = zoomRect.pos + zoomRect.size / 2;
			auto clampTo = gridRect;
			auto toCenter = r.Inset(-2);
			clampTo.size -= zoomedRect.size - Gui::View::Size2{ 1, 1 };
			auto thumbnailAt = Gui::View::RobustClamp(center - toCenter.size / 2, clampTo);
			view.ForcePosition(thumbnailAt);
		}
	}
}
