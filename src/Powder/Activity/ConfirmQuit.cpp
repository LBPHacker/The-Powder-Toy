#include "ConfirmQuit.hpp"
#include "Gui/Host.hpp"
#include "Lang/Translation.hpp"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size dialogWidth = 300;
	}

	void ConfirmQuit::Gui()
	{
		SetAllowGlobalQuit(false);
		auto confirmquit = ScopedDialog("confirmquit", "DEFAULT_LANG_CONFIRMQUIT"_St, dialogWidth);
		SetSize(50);
		Text("question", "Are you sure you want to exit the game?");
	}

	ConfirmQuit::DispositionFlags ConfirmQuit::GetDisposition() const
	{
		return DispositionFlags::none;
	}

	void ConfirmQuit::Ok()
	{
		GetHost().Stop();
		Exit();
	}
}
