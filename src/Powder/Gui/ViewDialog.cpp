#include "View.hpp"
#include "Host.hpp"

namespace Powder::Gui
{
	void View::BeginDialog(ComponentKey key, StringView title, std::optional<int32_t> size)
	{
		SetExitWhenRootMouseDown(true);
		BeginModal(key);
		if (size)
		{
			SetSize(*size);
		}
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		BeginText("title", title, Gui::View::TextFlags::none);
		SetSize(21);
		SetTextPadding(7);
		EndText();
		Separator("dialogbeginsep");
		BeginVPanel("content");
		SetPadding(6);
		SetSpacing(4);
	}

	bool View::EndDialog(bool allowOk)
	{
		bool tryOk = false;
		EndPanel();
		Separator("dialogendsep");
		BeginHPanel("dialogend");
		{
			SetSize(27);
			SetPadding(6);
			SetSpacing(4);
			if (Button("Cancel", 50)) // TODO-REDO_UI-TRANSLATE
			{
				Exit();
			}
			if (Button("\boOK", SpanAll{}, ButtonFlags::none)) // TODO-REDO_UI-TRANSLATE
			{
				SetEnabled(allowOk);
				if (allowOk)
				{
					tryOk = true;
					Exit();
				}
			}
		}
		EndPanel();
		EndModal();
		return tryOk;
	}
}
