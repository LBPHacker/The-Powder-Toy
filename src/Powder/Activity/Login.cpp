#include "Login.hpp"
#include "Game.hpp"
#include "Gui/Host.hpp"
#include "RequestView.hpp"
#include "client/http/LoginRequest.h"
#include "client/Client.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size dialogWidth = 200;
	}

	Login::Login(Game &newGame) : View(newGame.GetHost()), game(newGame)
	{
	}

	void Login::Gui()
	{
		auto login = ScopedDialog("login", "Sign in", dialogWidth); // TODO-REDO_UI-TRANSLATE
		BeginTextbox("username", username, "[username]", TextboxFlags::none); // TODO-REDO_UI-TRANSLATE
		SetSize(Common{});
		EndTextbox();
		BeginTextbox("password", password, "[password]", TextboxFlags::password); // TODO-REDO_UI-TRANSLATE
		SetSize(Common{});
		EndTextbox();
	}

	Login::DispositionFlags Login::GetDisposition() const
	{
		if (!username.empty() && !password.empty())
		{
			return DispositionFlags::none;
		}
		return DispositionFlags::okDisabled;
	}

	void Login::Ok()
	{
		auto request = std::make_unique<http::LoginRequest>(username, password);
		auto requestView = MakeRequestView(GetHost(), "Signing in", std::move(request)); // TODO-REDO_UI-TRANSLATE
		loginInfoFuture = requestView->GetFuture();
		requestView->Start();
		PushAboveThis(requestView);
	}

	void Login::HandleTick()
	{
		if (loginInfoFuture.valid() && loginInfoFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			try
			{
				auto info = loginInfoFuture.get();
				Client::Ref().SetAuthUser(info.user);
				for (auto &notification : info.notifications)
				{
					game.AddServerNotification(notification);
				}
				Exit();
			}
			catch (const http::RequestError &ex)
			{
			}
		}
	}
}
