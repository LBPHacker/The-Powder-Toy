#include "View.hpp"
#include "Host.hpp"

namespace Powder::Gui
{
	void View::BeginPanel(ComponentKey key)
	{
		BeginComponent(key);
	}

	void View::BeginHPanel(ComponentKey key)
	{
		BeginPanel(key);
		SetPrimaryAxis(Axis::horizontal);
	}

	void View::BeginVPanel(ComponentKey key)
	{
		BeginPanel(key);
		SetPrimaryAxis(Axis::vertical);
	}

	void View::EndPanel()
	{
		EndComponent();
	}
}
