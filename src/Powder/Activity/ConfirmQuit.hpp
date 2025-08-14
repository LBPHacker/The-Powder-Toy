#pragma once
#include "Gui/View.hpp"

namespace Powder::Activity
{
	class ConfirmQuit : public Gui::View
	{
		void Gui() final override;
		CanApplyKind CanApply() const final override;
		void Apply() final override;
		bool Cancel() final override;

	public:
		using View::View;
	};
}
