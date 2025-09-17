#pragma once
#include "Gui/ComplexInput.hpp"
#include "Gui/View.hpp"

namespace Powder::Activity
{
	class Game;

	class Settings : public Gui::View
	{
		Game &game;

		void Gui() final override;
		DispositionFlags GetDisposition() const final override;
		void Ok() final override;
		bool Cancel() final override;

		struct ScaleOption
		{
			std::string name;
			int32_t scale;
		};
		std::vector<ScaleOption> scaleOptions;
		int32_t scaleIndex = 0;

		int32_t currentCategory = 0;
		void GuiSimulation();
		void GuiAmbientAirTemp();
		void GuiVideo();

		Gui::NumberInputContext<float> ambientAirTempInput;
		std::optional<float> ParseTemperature(const std::string &str) const;

	public:
		Settings(Game &newGame);
	};
}
