#include "ConfirmQuit.hpp"
#include "Gui/Host.hpp"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size dialogWidth = 300;
	}

	void ConfirmQuit::Gui()
	{
		SetAllowGlobalQuit(false);
		auto confirmquit = ScopedDialog("confirmquit", "Quit game", dialogWidth); // TODO-REDO_UI-TRANSLATE
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
