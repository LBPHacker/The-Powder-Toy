#include "CheckBox.h"

#include "SDLWindow.h"
#include "Appearance.h"

namespace gui
{
	void CheckBox::Draw() const
	{
		auto abs = AbsolutePosition();
		auto &cc = Appearance::colors.inactive;
		auto &c = Enabled() ? (UnderMouse() ? cc.hover : cc.idle) : cc.disabled;
		auto &g = SDLWindow::Ref();
		g.DrawRect(Rect{ abs + Point{ 0, 1 }, Point{ 12, 12 } }, c.body);
		g.DrawRectOutline(Rect{ abs + Point{ 0, 1 }, Point{ 12, 12 } }, c.border);
		if (value)
		{
			g.DrawRect(Rect{ abs + Point{ 3, 4 }, Point{ 6, 6 } }, c.border);
		}
		g.DrawText(abs + drawTextAt, text, c.text);
	}

	void CheckBox::MouseClick(Point current, int button, int clicks)
	{
		if (button == SDL_BUTTON_LEFT)
		{
			value = !value;
			if (change)
			{
				change(value);
			}
		}
	}

	void CheckBox::Size(Point newSize)
	{
		Static::Size(newSize);
		TextRect(Rect{ Point{ 14, 0 }, newSize - Point{ 14, 0 } });
	}
}
