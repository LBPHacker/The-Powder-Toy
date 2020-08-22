#pragma once

#include "gui/PopupWindow.h"

#include "common/String.h"

namespace gui
{
	class Button;
	class Label;
}

namespace activities
{
namespace login
{
	class IconTextBox;

	class Login : public gui::PopupWindow
	{
		gui::Button *signInButton = nullptr;
		gui::Button *signOutButton = nullptr;
		IconTextBox *usernameBox = nullptr;
		IconTextBox *passwordBox = nullptr;
		gui::Label *statusLabel = nullptr;
		String statusText;

		void Update();

	public:
		Login();
	};
}
}
