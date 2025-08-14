#include "View.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"

namespace Powder::Gui
{
	void View::BeginButton(ComponentKey key, StringView text, ButtonFlags buttonFlags, Rgba8 color)
	{
		BeginComponent(key);
		auto &g = GetHost();
		auto edgeColor = 0xFFFFFFFF_argb;
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
			}
			else if (ButtonFlagBase(buttonFlags) & ButtonFlagBase(ButtonFlags::stuck))
			{
				g.FillRect(component.rect, edgeColor);
				color = InvertColor(color.NoAlpha()).WithAlpha(color.Alpha);
			}
			else if (IsHovered())
			{
				auto backgroundColor = edgeColor;
				if (IsMouseDown(SDL_BUTTON_LEFT) || IsClicked(SDL_BUTTON_LEFT))
				{
					color = InvertColor(color.NoAlpha()).WithAlpha(color.Alpha);
				}
				else
				{
					backgroundColor.Alpha /= 2;
				}
				g.FillRect(component.rect, backgroundColor);
			}
		}
		if (!text.view.empty())
		{
			auto textFlags = TextFlags::none;
			if (ButtonFlagBase(buttonFlags) & ButtonFlagBase(ButtonFlags::autoWidth))
			{
				textFlags = textFlags | TextFlags::autoWidth;
			}
			BeginText(key, text, textFlags, color);
			auto &component = *GetParentComponent();
			SetTextPadding(
				component.prevContent.textPaddingBefore.X,
				component.prevContent.textPaddingAfter.X,
				component.prevContent.textPaddingBefore.Y,
				component.prevContent.textPaddingAfter.Y
			);
			EndText();
		}
		g.DrawRect(GetRect(), edgeColor);
	}

	bool View::EndButton()
	{
		bool clicked = IsClicked(SDL_BUTTON_LEFT);
		EndComponent();
		return clicked;
	}

	bool View::Button(StringView text, SetSizeSize size, ButtonFlags buttonFlags)
	{
		return Button(text, text, size, buttonFlags);
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
		auto rr = GetRect();
		auto &g = GetHost();
		g.FillRect(rr.Inset(1), color.WithAlpha(255));
	}

	bool View::EndColorButton()
	{
		return EndButton();
	}
}
