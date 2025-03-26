#pragma once
#include "Gui/View.hpp"
#include "Common/Color.hpp"

namespace Powder::Activity
{
	class Game;

	class AddLife : public Gui::View
	{
		Game &game;
		int toolSelection;
		int rule;
		Rgb8 color1;
		Rgb8 color2;
		std::string name;
		std::string ruleString;

		bool HandleEvent(const SDL_Event &event) final override;
		void Gui() final override;

	public:
		AddLife(Game &newGame, int newToolSelection, int newRule, Rgb8 newColor1, Rgb8 newColor2);

		bool CanTryOk() const;
		void Ok();
	};
}
