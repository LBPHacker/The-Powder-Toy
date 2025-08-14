#pragma once
#include "simulation/AccessProperty.h"
#include "Tool.h"
#include <optional>

namespace Powder::Activity
{
	class Game;
	class Property;
}

class PropertyTool: public Tool
{
public:
	struct Configuration
	{
		AccessProperty changeProperty;
		String propertyValueStr;
	};

private:
	void SetProperty(Simulation *sim, ui::Point position);
	void SetConfiguration(std::optional<Configuration> newConfiguration);

	Powder::Activity::Game &game;
	std::optional<Configuration> configuration;
	std::optional<int> takePropertyFrom;

	friend class PropertyWindow;
	friend class Powder::Activity::Property;

public:
	PropertyTool(Powder::Activity::Game &newGame):
		Tool(0, "PROP", "Property Drawing Tool. Use to alter the properties of elements in the field.",
			0xFEA900_rgb, "DEFAULT_UI_PROPERTY", NULL
		), game(newGame)
	{}

	void QueueTakePropertyFrom(int newTakePropertyFrom)
	{
		takePropertyFrom = newTakePropertyFrom;
	}

	void OpenWindow();
	void Click(Simulation * sim, Brush const &brush, ui::Point position) override { }
	void Draw(Simulation *sim, Brush const &brush, ui::Point position) override;
	void DrawLine(Simulation * sim, Brush const &brush, ui::Point position1, ui::Point position2, bool dragging) override;
	void DrawRect(Simulation * sim, Brush const &brush, ui::Point position1, ui::Point position2) override;
	void DrawFill(Simulation * sim, Brush const &brush, ui::Point position) override;

	std::optional<Configuration> GetConfiguration() const
	{
		return configuration;
	}

	void Select(int toolSelection) final override;
};
