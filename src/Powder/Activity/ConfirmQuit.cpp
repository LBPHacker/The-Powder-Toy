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
		BeginDialog("confirmquit", "You are about to quit", GetHost().GetSize().OriginRect(), dialogWidth); // TODO-REDO_UI-TRANSLATE
		SetSize(50);
		Text("question", "Are you sure you want to exit the game?");
		EndDialog();
	}

	ConfirmQuit::CanApplyKind ConfirmQuit::CanApply() const
	{
		return CanApplyKind::yes;
	}

	void ConfirmQuit::Apply()
	{
		GetHost().Stop();
		Exit();
	}

	bool ConfirmQuit::Cancel()
	{
		Exit();
		return true;
	}
}
