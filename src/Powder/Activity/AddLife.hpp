#pragma once
#include "Gui/View.hpp"
#include "Common/Color.hpp"
#include "simulation/CustomGOLData.h"

namespace Powder::Activity
{
	class Game;

	class AddLife : public Gui::View
	{
		Game &game;
		int32_t toolSelection;
		int32_t rule;
		Rgb8 color1;
		Rgb8 color2;
		std::string name;
		std::string ruleString;

		void Gui() final override;
		DispositionFlags GetDisposition() const final override;
		void Ok() final override;

		std::optional<CustomGOLData> CheckCustomGolToAdd() const;

	public:
		AddLife(Game &newGame, int32_t newToolSelection, int32_t newRule, Rgb8 newColor1, Rgb8 newColor2);
	};
}
