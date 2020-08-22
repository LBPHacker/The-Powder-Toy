#pragma once

#include "TextBox.h"

namespace gui
{
	class Label : public TextBox
	{
	public:
		Label()
		{
			DrawBody(false);
			Editable(false);
		}
	};
}
