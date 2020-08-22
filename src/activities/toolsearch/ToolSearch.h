#pragma once

#include "gui/PopupWindow.h"

#include <vector>

namespace gui
{
	class ScrollPanel;
	class TextBox;
}

namespace activities
{
namespace toolsearch
{
	class ToolSearchButton;

	class ToolSearch : public gui::PopupWindow
	{
		std::vector<ToolSearchButton *> buttons;
		gui::TextBox *searchBox = nullptr;
		gui::ScrollPanel *sp = nullptr;

		void Update();

	public:
		ToolSearch();

		void Draw() const final override;
	};
}
}
