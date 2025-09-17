#include "View.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"

namespace Powder::Gui
{
	void View::BeginButton(ComponentKey key, StringView text, ButtonFlags buttonFlags, Rgba8 color, Rgba8 backgroundColor)
	{
		BeginComponent(key);
		SetTextPadding(4, 3);
		auto &g = GetHost();
		auto edgeColor = 0xFFFFFFFF_argb;
		auto onBackground = [backgroundColor](Rgba8 color) {
			return backgroundColor.Blend(color);
		};
		{
			auto &component = *GetCurrentComponent();
			if (component.prevContent.enabled)
			{
				SetHandleButtons(true);
			}
			if (!component.prevContent.enabled)
			{
				edgeColor.Alpha /= 2;
				color.Alpha /= 2;
				backgroundColor.Alpha /= 2;
				if (backgroundColor != 0x00000000_argb)
				{
					g.FillRect(component.rect, backgroundColor);
				}
			}
			else if (ButtonFlagBase(buttonFlags) & ButtonFlagBase(ButtonFlags::stuck))
			{
				g.FillRect(component.rect, edgeColor);
				color = InvertColor(color.NoAlpha()).WithAlpha(color.Alpha);
			}
			else if (IsHovered())
			{
				auto hoverColor = edgeColor;
				if (IsMouseDown(SDL_BUTTON_LEFT) || IsClicked(SDL_BUTTON_LEFT))
				{
					color = InvertColor(color.NoAlpha()).WithAlpha(color.Alpha);
				}
				else
				{
					hoverColor.Alpha /= 2;
				}
				g.FillRect(component.rect, onBackground(hoverColor));
			}
			else if (backgroundColor != 0x00000000_argb)
			{
				g.FillRect(component.rect, backgroundColor);
			}
		}
		std::optional<StringView> truncateText;
		auto textFlags = TextFlags::none;
		if (ButtonFlagBase(buttonFlags) & ButtonFlagBase(ButtonFlags::autoWidth))
		{
			textFlags = textFlags | TextFlags::autoWidth;
		}
		else if (ButtonFlagBase(buttonFlags) & ButtonFlagBase(ButtonFlags::truncateText))
		{
			truncateText = StringView("...");
		}
		if (!text.view.empty())
		{
			BeginTextInternal(key, text, truncateText, std::nullopt, textFlags, color);
			auto &component = *GetParentComponent();
			SetTextPadding(
				component.prevContent.textPaddingBefore.X,
				component.prevContent.textPaddingAfter.X,
				component.prevContent.textPaddingBefore.Y,
				component.prevContent.textPaddingAfter.Y
			);
			EndText();
		}
		g.DrawRect(GetRect(), 0x000000_rgb .Blend(edgeColor).WithAlpha(255));
	}

	bool View::EndButton()
	{
		bool clicked = IsClicked(SDL_BUTTON_LEFT);
		auto enabledNow = GetCurrentComponent()->content.enabled;
		EndComponent();
		// Slight hack: clicked can be true even if content.enabled is false because it only takes prevContent.enabled
		//              into account. This is fine for most components but buttons are special in that you might
		//              very well expect them to not return true if they're disabled, so you might put code behind them
		//              that causes trouble if run unexpectedly.
		return clicked && enabledNow;
	}

	bool View::Button(ComponentKey key, StringView text, SetSizeSize size, ButtonFlags buttonFlags)
	{
		BeginButton(key, text, buttonFlags);
		SetSize(size);
		return EndButton();
	}

	void View::BeginColorButton(ComponentKey key, Rgb8 color)
	{
		BeginButton(key, "", ButtonFlags::none);
		SetCursor(Cursor::hand);
		auto rr = GetRect();
		auto &g = GetHost();
		g.FillRect(rr.Inset(1), color.WithAlpha(255));
	}

	bool View::EndColorButton()
	{
		return EndButton();
	}
}
