#include "Separator.h"

#include "SDLWindow.h"
#include "Appearance.h"

namespace gui
{
	void Separator::Draw() const
	{
		auto abs = AbsolutePosition();
		auto &g = SDLWindow::Ref();
		g.DrawLine(abs, abs + Point{ Size().x - 1, 0 }, Appearance::colors.inactive.idle.border);
	}
}
