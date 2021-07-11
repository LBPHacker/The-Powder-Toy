#include "Login.h"

#include "activities/game/Game.h"
#include "gui/Appearance.h"
#include "gui/Button.h"
#include "gui/TextBox.h"
#include "gui/Label.h"
#include "gui/Static.h"
#include "gui/Separator.h"
#include "gui/SDLWindow.h"
#include "gui/Icons.h"
#include "client/Client.h"
#include "graphics/FontData.h"

namespace activities
{
namespace login
{
	constexpr auto parentSize = gui::Point{ WINDOWW, WINDOWH };
	constexpr auto windowSize = gui::Point{ 200, 93 };
	constexpr auto maxStatusLines = 8;

	class IconTextBox : public gui::TextBox
	{
		int iconPadding = 0;
		String iconString;

		void UpdateTextRect()
		{
			TextRect(gui::Rect({ gui::Point{ iconPadding, 0 }, Size() - gui::Point{ iconPadding, 0 } }));
		}

	public:
		void Size(gui::Point newSize) final override
		{
			Component::Size(newSize);
			UpdateTextRect();
		}
		gui::Point Size() const
		{
			return Component::Size();
		}

		void Draw() const final override
		{
			auto &cc = gui::Appearance::colors.inactive;
			auto &c = Enabled() ? (UnderMouse() ? cc.hover : cc.idle) : cc.disabled;
			TextBox::Draw();
			gui::SDLWindow::Ref().DrawText(AbsolutePosition() + gui::Point{ 3, 3 }, iconString, c.text);
		}

		void IconString(String newIconString)
		{
			iconString = newIconString;
		}
		const String &IconString() const
		{
			return iconString;
		}

		void IconPadding(int newIconPadding)
		{
			iconPadding = newIconPadding;
			UpdateTextRect();
		}
		const int IconPadding() const
		{
			return iconPadding;
		}
	};

	Login::Login()
	{
		auto title = EmplaceChild<gui::Static>();
		title->Position(gui::Point{ 6, 5 });
		title->Size(gui::Point{ windowSize.x - 20, 14 });
		title->Text("Server login");
		title->TextColor(gui::Appearance::informationTitle);
		auto sep = EmplaceChild<gui::Separator>();
		sep->Position(gui::Point{ 1, 22 });
		sep->Size(gui::Point{ windowSize.x - 2, 1 });

		signInButton = EmplaceChild<gui::Button>().get();
		signInButton->Text("\boSign in");
		signInButton->Click([this]() {
			auto &username = usernameBox->Text();
			auto &password = passwordBox->Text();
			if (!username.size() || !password.size())
			{
				return;
			}
			if (username.Contains("@"))
			{
				statusText = "Use your Powder Toy account to log in, not your email. If you don't have a Powder Toy account, you can create one at https://powdertoy.co.uk/Register.html";
			}
			else
			{
				// * TODO: Make this async once Client gets refactored.
				User user(0, "");
				Client::Ref().Login(username.ToUtf8(), password.ToUtf8(), user);
				if (user.UserID)
				{
					Client::Ref().SetAuthUser(user);
					game::Game::Ref().UpdateLoginButton();
					Quit();
				}
				else
				{
					statusText = Client::Ref().GetLastError();
				}
			}
			Update();
		});
		OkayButton(GetChild(signInButton));

		signOutButton = EmplaceChild<gui::Button>().get();
		signOutButton->Text("Sign out");
		signOutButton->Click([this]() {
			Client::Ref().SetAuthUser(User(0, ""));
			game::Game::Ref().UpdateLoginButton();
			Quit();
		});

		usernameBox = EmplaceChild<IconTextBox>().get();
		usernameBox->IconString(
			gui::ColorString(gui::Color{ 0x20, 0x40, 0x80 }) + gui::IconString(gui::Icons::userbody   , gui::Point{ 1, -1 }) + gui::StepBackString() +
			gui::ColorString(gui::Color{ 0xFF, 0xFF, 0xFF }) + gui::IconString(gui::Icons::useroutline, gui::Point{ 1, -1 })
		);
		usernameBox->IconPadding(16);
		usernameBox->PlaceHolder("[username]");
		usernameBox->Position(gui::Point{ 8, 31 });
		usernameBox->Size(gui::Point{ windowSize.x - 16, 17 });

		passwordBox = EmplaceChild<IconTextBox>().get();
		passwordBox->IconString(
			gui::ColorString(gui::Color{ 0xA0, 0x90, 0x20 }) + gui::IconString(gui::Icons::passbody   , gui::Point{ 1, -1 }) + gui::StepBackString() +
			gui::ColorString(gui::Color{ 0xFF, 0xFF, 0xFF }) + gui::IconString(gui::Icons::passoutline, gui::Point{ 1, -1 })
		);
		passwordBox->IconPadding(16);
		passwordBox->PlaceHolder("[password]");
		passwordBox->Position(gui::Point{ 8, 52 });
		passwordBox->Size(gui::Point{ windowSize.x - 16, 17 });
		passwordBox->AllowCopy(false);
		passwordBox->Format([](const String &unformatted) -> String {
			return String(unformatted.size(), gui::Icons::maskchar);
		});

		statusLabel = EmplaceChild<gui::Label>().get();
		statusLabel->Position(gui::Point{ 8, 73 });
		statusLabel->Multiline(true);
		statusLabel->TextColor(gui::Appearance::errorTitle);

		usernameBox->Focus();
		if (Client::Ref().GetAuthUser().UserID)
		{
			usernameBox->Text(Client::Ref().GetAuthUser().Username.FromUtf8());
			usernameBox->SelectAll();
		}
		Update();
	}

	void Login::Update()
	{
		statusLabel->Visible(false);
		signOutButton->Visible(false);
		auto targetSize = windowSize;
		if (statusText.size())
		{
			statusLabel->Visible(true);
			statusLabel->Text(statusText);
			// * Update size first so our next reading of Lines will be accurate.
			statusLabel->Size(gui::Point{ targetSize.x - 16, 17 });
			auto lines = statusLabel->Lines();
			if (lines > maxStatusLines)
			{
				lines = maxStatusLines;
			}
			statusLabel->Size(gui::Point{ targetSize.x - 16, 5 + FONT_H * lines });
			targetSize.y += 6 + FONT_H * lines;
		}
		if (Client::Ref().GetAuthUser().UserID)
		{
			signOutButton->Visible(true);
			auto floorW2 = targetSize.x / 2;
			auto ceilW2 = targetSize.x - floorW2;
			signOutButton->Position(gui::Point{ 0, targetSize.y - 16 });
			signOutButton->Size(gui::Point{ ceilW2, 16 });
			signInButton->Position(gui::Point{ ceilW2 - 1, targetSize.y - 16 });
			signInButton->Size(gui::Point{ floorW2 + 1, 16 });
		}
		else
		{
			signInButton->Position(gui::Point{ 0, targetSize.y - 16 });
			signInButton->Size(gui::Point{ targetSize.x, 16 });
		}
		Size(targetSize);
		Position((parentSize - targetSize) / 2);
	}
}
}
