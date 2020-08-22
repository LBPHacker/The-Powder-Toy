#include "PopupWindow.h"

#include "SDLWindow.h"
#include "Appearance.h"

namespace gui
{
	void PopupWindow::Draw() const
	{
		auto abs = AbsolutePosition();
		auto size = Size();
		auto &c = Appearance::colors.inactive.idle;
		auto &g = SDLWindow::Ref();
		g.DrawRect(Rect{ abs, size }, c.body);
		g.DrawRectOutline(Rect{ abs, size }, c.border);
		g.DrawRectOutline(Rect{ abs - Point{ 1, 1 }, size + Point{ 2, 2 } }, c.shadow);
		ModalWindow::Draw();
	}

	bool PopupWindow::WantsBackdrop() const
	{
		return true;
	}
}
