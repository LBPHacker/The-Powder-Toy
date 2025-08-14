#include "View.hpp"
#include "Host.hpp"

namespace Powder::Gui
{
	namespace
	{
		constexpr Gui::View::Size2 commondialogSize = { 250, 80 };
	}

	void View::BeginDialog(ComponentKey key, StringView title, Rect rootRect, std::optional<Size> size)
	{
		SetExitWhenRootMouseDown(true);
		BeginModal(key, rootRect);
		if (size)
		{
			SetSize(*size);
		}
		SetTextAlignment(Alignment::left, Alignment::center);
		BeginText("title", title, TextFlags::none);
		SetSize(GetHost().GetCommonMetrics().heightToFitSize);
		SetTextPadding(7);
		EndText();
		Separator("dialogbeginsep");
		BeginVPanel("content");
		SetPadding(Common{});
		SetSpacing(Common{});
	}

	void View::EndDialog()
	{
		auto canApplyKind = CanApply();
		EndPanel();
		Separator("dialogendsep");
		BeginHPanel("dialogend");
		{
			SetSize(GetHost().GetCommonMetrics().heightToFitSize);
			SetPadding(Common{});
			SetSpacing(Common{});
			auto onlyCancel = canApplyKind == CanApplyKind::onlyCancel;
			auto canApply   = canApplyKind == CanApplyKind::yes ||
			                  canApplyKind == CanApplyKind::onlyApply;
			if (canApplyKind != CanApplyKind::onlyApply)
			{
				BeginButton("cancel", "Cancel", ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
				if (!onlyCancel)
				{
					SetSize(GetHost().GetCommonMetrics().smallButton);
				}
				if (EndButton())
				{
					Cancel();
				}
			}
			if (!onlyCancel)
			{
				BeginButton("ok", "\boOK", ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
				SetSize(SpanAll{});
				SetEnabled(canApply);
				if (EndButton() && canApply)
				{
					Apply();
				}
			}
		}
		EndPanel();
		EndModal();
	}

	void View::BeginCommondialog(ComponentKey key, StringView title, CommondialogState state)
	{
		BeginDialog(key, title, GetHost().GetSize().OriginRect(), commondialogSize.X);
		SetSize(commondialogSize.Y);
		if (auto *failed = std::get_if<CommondialogStateFailed>(&state))
		{
			SetPadding(0);
			BeginScrollpanel("reasonpanel");
			SetPadding(GetHost().GetCommonMetrics().padding + 1);
			SetMaxSize(MaxSizeFitParent{});
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			BeginText("reason", failed->reason, TextFlags::multiline | TextFlags::selectable | TextFlags::autoHeight);
			EndText();
			EndScrollpanel();
		}
		else
		{
			Spinner("progress", GetHost().GetCommonMetrics().bigSpinner);
		}
	}

	void View::EndCommondialog()
	{
		EndDialog();
	}

	void View::Commondialog(ComponentKey key, StringView title, CommondialogState state)
	{
		BeginCommondialog(key, title, state);
		EndDialog();
	}
}
