#include "View.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"

namespace Powder::Gui
{
	void View::BeginCheckbox(ComponentKey key, StringView text, bool &checked, CheckboxFlags checkboxFlags, Rgba8 color)
	{
		constexpr Size boxSize    = 12;
		constexpr Size boxSpacing = 4;
		auto &g = GetHost();
		auto edgeColor = 0xFFFFFFFF_argb;
		BeginComponent(key);
		SetPrimaryAxis(Axis::vertical);
		auto enabled = GetCurrentComponent()->prevContent.enabled;
		if (enabled)
		{
			SetHandleButtons(true);
		}
		if (!enabled)
		{
			edgeColor.Alpha /= 2;
			color.Alpha /= 2;
		}
		if (!text.view.empty())
		{
			auto textFlags = TextFlags::none;
			if (CheckboxFlagBase(checkboxFlags) & CheckboxFlagBase(CheckboxFlags::multiline))
			{
				textFlags = textFlags | TextFlags::autoHeight | TextFlags::multiline;
			}
			else
			{
				if (CheckboxFlagBase(checkboxFlags) & CheckboxFlagBase(CheckboxFlags::autoWidth))
				{
					textFlags = textFlags | TextFlags::autoWidth;
				}
			}
			SetTextAlignment(Alignment::left, Alignment::center);
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
		auto r = GetRect();
		SetPadding(0, 0, boxSize + boxSpacing, 0);
		Rect rr{ r.TopLeft() + Pos2{ 0, 0 }, { boxSize, boxSize } };
		auto round = bool(CheckboxFlagBase(checkboxFlags) & CheckboxFlagBase(CheckboxFlags::round));
		if (round)
		{
			g.DrawLine(rr.pos + Pos2{  4,  0 }, rr.pos + Pos2{  7,  0 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  8,  1 }, rr.pos + Pos2{  9,  1 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{ 10,  2 }, rr.pos + Pos2{ 10,  3 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{ 11,  4 }, rr.pos + Pos2{ 11,  7 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{ 10,  8 }, rr.pos + Pos2{ 10,  9 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  8, 10 }, rr.pos + Pos2{  9, 10 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  4, 11 }, rr.pos + Pos2{  7, 11 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  2, 10 }, rr.pos + Pos2{  3, 10 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  1,  8 }, rr.pos + Pos2{  1,  9 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  0,  4 }, rr.pos + Pos2{  0,  7 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  1,  2 }, rr.pos + Pos2{  1,  3 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  2,  1 }, rr.pos + Pos2{  3,  1 }, edgeColor);
		}
		else
		{
			g.DrawRect(rr, edgeColor);
		}
		if (IsClicked(SDL_BUTTON_LEFT))
		{
			checked = !checked;
		}
		if (checked)
		{
			if (round)
			{
				g.DrawLine(rr.pos + Pos2{ 3, 4 }, rr.pos + Pos2{ 3, 7 }, edgeColor);
				g.DrawLine(rr.pos + Pos2{ 4, 3 }, rr.pos + Pos2{ 4, 8 }, edgeColor);
				g.DrawLine(rr.pos + Pos2{ 5, 3 }, rr.pos + Pos2{ 5, 8 }, edgeColor);
				g.DrawLine(rr.pos + Pos2{ 6, 3 }, rr.pos + Pos2{ 6, 8 }, edgeColor);
				g.DrawLine(rr.pos + Pos2{ 7, 3 }, rr.pos + Pos2{ 7, 8 }, edgeColor);
				g.DrawLine(rr.pos + Pos2{ 8, 4 }, rr.pos + Pos2{ 8, 7 }, edgeColor);
			}
			else
			{
				g.FillRect(rr.Inset(3), edgeColor);
			}
		}
	}

	bool View::EndCheckbox()
	{
		bool clicked = IsClicked(SDL_BUTTON_LEFT);
		EndComponent();
		return clicked;
	}

	bool View::Checkbox(StringView text, bool &checked, SetSizeSize size, CheckboxFlags checkboxFlags)
	{
		return Checkbox(text, text, checked, size, checkboxFlags);
	}

	bool View::Checkbox(ComponentKey key, StringView text, bool &checked, SetSizeSize size, CheckboxFlags checkboxFlags)
	{
		BeginCheckbox(key, text, checked, checkboxFlags);
		SetSize(size);
		return EndCheckbox();
	}
}
