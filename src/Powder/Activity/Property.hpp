#pragma once
#include "Gui/View.hpp"
#include "gui/game/tool/PropertyTool.h"
#include <optional>

class PropertyTool;

namespace Powder::Activity
{
	class Game;

	class Property : public Gui::View
	{
		using PropertyIndex = int32_t;

		PropertyTool &tool;
		Game &game;
		PropertyIndex propIndex = 0;
		std::string propValueStr;
		std::optional<PropertyTool::Configuration> configuration;
		bool focusPropValue = true;

		bool HandleEvent(const SDL_Event &event) final override;
		void Gui() final override;

		std::optional<std::pair<PropertyIndex, std::string>> TakePropertyFrom(const Simulation &sim, std::optional<int> i) const;
		void Update();

	public:
		Property(PropertyTool &newTool, Game &newGame, std::optional<int> takePropertyFrom);

		bool CanTryOk() const;
		void Ok();
	};
}
