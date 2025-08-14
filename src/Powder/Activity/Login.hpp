#pragma once
#include "Gui/View.hpp"
#include "client/LoginInfo.h"
#include <future>

namespace Powder::Activity
{
	class Game;

	class Login : public Gui::View
	{
		Game &game;
		std::string username;
		std::string password;
		std::future<LoginInfo> loginInfoFuture;

		void Gui() final override;
		CanApplyKind CanApply() const final override;
		void Apply() final override;
		bool Cancel() final override;
		void HandleTick() final override;

	public:
		Login(Game &newGame);
	};
}
