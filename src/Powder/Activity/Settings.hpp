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

		template<class Number>
		struct NumberInputContext
		{
			std::optional<Number> editedNumber;
			std::string str;
			bool wantReset = true;
		};
		NumberInputContext<float> ambientAirTempInput;
		std::optional<float> ParseTemperature(const std::string &str) const;

	public:
		Settings(Game &newGame);

		bool CanTryOk() const;
		void Ok();
	};
}
