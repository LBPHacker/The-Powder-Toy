#pragma once
#include "Gui/View.hpp"

namespace Powder::Activity
{
	class ConfirmQuit : public Gui::View
	{
		void Gui() final override;
		DispositionFlags GetDisposition() const final override;
		void Ok() final override;

	public:
		using View::View;
	};
}
