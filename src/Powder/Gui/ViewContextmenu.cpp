#include "View.hpp"
#include "SdlAssert.hpp"

namespace Powder::Gui
{
	bool View::MaybeBeginContextmenu(ComponentKey key)
	{
		SetHandleButtons(true);
		if (MaybeBeginPopup(key, IsClicked(SDL_BUTTON_RIGHT)))
		{
			SetPadding(1);
			SetSpacing(-1);
			return true;
		}
		return false;
	}

	void View::EndContextmenu()
	{
		EndPopup();
	}

	bool View::ContextmenuButton(ComponentKey key, StringView item)
	{
		BeginButton(key, item, ButtonFlags::autoWidth);
		SetTextPadding(6);
		SetSize(Common{});
		if (EndButton())
		{
			PopupClose();
			return true;
		}
		return false;
	}
}
