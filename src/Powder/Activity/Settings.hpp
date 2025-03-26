#pragma once
#include "Gui/View.hpp"
#include <optional>

namespace Powder::Activity
{
	class Game;

	class Settings : public Gui::View
	{
		Game &game;

		bool HandleEvent(const SDL_Event &event) final override;
		void Gui() final override;

	public:
		Settings(Game &newGame);

		bool CanTryOk() const;
		void Ok();
	};
}
