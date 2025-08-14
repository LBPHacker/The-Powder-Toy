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

		void Gui() final override;
		CanApplyKind CanApply() const final override;
		void Apply() final override;
		bool Cancel() final override;

	public:
		AddLife(Game &newGame, int newToolSelection, int newRule, Rgb8 newColor1, Rgb8 newColor2);
	};
}
