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

		template<class Getter, class Setter>
		void Checkbox(ComponentKey key, int indentation, StringView title, Getter getter, Setter setter);
		template<class Getter, class Setter, class Items>
		void Dropdown(ComponentKey key, StringView title, Getter getter, Setter setter, Items &&items);

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
