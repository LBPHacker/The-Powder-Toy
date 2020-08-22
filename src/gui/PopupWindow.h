#pragma once

#include "ModalWindow.h"

namespace gui
{
	class PopupWindow : public ModalWindow
	{
	public:
		void Draw() const override;
		
		bool WantsBackdrop() const final override;
	};
}
