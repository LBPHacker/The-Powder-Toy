#pragma once

#include "TextBox.h"

namespace gui
{
	class Label : public TextBox
	{
		String formattedText;

	public:
		Label();
		void Text(String newText) final override;
	};
}
