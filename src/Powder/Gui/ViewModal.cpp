#include "View.hpp"
#include "Host.hpp"

namespace Powder::Gui
{
	void View::BeginModal(ComponentKey key)
	{
		BeginComponent(key);
		BeginVPanel("inner");
		SetParentFillRatio(0);
		SetParentFillRatioSecondary(0);
		auto &component = *GetCurrentComponent();
		auto &g = GetHost();
		auto rh = component.rect.Inset(1); // TODO-REDO_UI: child clip rect
		g.FillRect(component.rect, 0xFF000000_argb);
		g.DrawRect(rh, 0xFFFFFFFF_argb);
		SetPadding(2);
	}

	void View::EndModal()
	{
		EndPanel();
		EndComponent();
	}
}
