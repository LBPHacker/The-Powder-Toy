#include "Label.h"

namespace gui
{
	Label::Label()
	{
		DrawBody(false);
		Editable(false);
		Format([this](const String &) {
			return formattedText;
		});
	}

	void Label::Text(String newText)
	{
		formattedText = newText;
		TextBox::Text(newText);
	}
}
