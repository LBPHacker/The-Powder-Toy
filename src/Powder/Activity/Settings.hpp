#pragma once
#include "Gui/ComplexInput.hpp"
#include "Gui/InputMapper.hpp"
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
		bool HandleEvent(const SDL_Event &event) final override;

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
		void GuiShortcuts();

		Gui::NumberInputContext<float> ambientAirTempInput;
		std::optional<float> ParseTemperature(const std::string &str) const;

		int32_t selectedContext = 0;

	public:
		struct HumanReadableInputToAction
		{
			Gui::InputMapper::InputToAction inputToAction;
			std::string humanReadableInput;
		};
		struct InputMappingsByAction
		{
			std::shared_ptr<Gui::InputMapper::Action> action;
			std::vector<HumanReadableInputToAction> inputToActions;
		};
		struct InputMappingsByActionGroup
		{
			std::shared_ptr<Gui::InputMapper::ActionGroup> actionGroup;
			std::vector<InputMappingsByAction> inputMappings;
		};
		using InputMappings = std::vector<InputMappingsByActionGroup>;

	private:
		std::optional<InputMappings> inputMappingsForContext;

	public:
		Settings(Game &newGame);
	};
}
