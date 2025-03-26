#pragma once
#include "Tool.h"

namespace Powder::Activity
{
	class Game;
}

class GOLTool: public Tool
{
	Powder::Activity::Game &game;
public:
	GOLTool(Powder::Activity::Game &newGame):
		Tool(0, "CUST", "Add a new custom GOL type. (Use ctrl+shift+rightclick to remove them)",
			0xFEA900_rgb, "DEFAULT_UI_ADDLIFE", NULL
		),
		game(newGame)
	{
		MenuSection = SC_LIFE;
	}

	void OpenWindow(int toolSelection, int rule = 0, RGB colour1 = 0x000000_rgb, RGB colour2 = 0x000000_rgb);
	void Click(Simulation * sim, Brush const &brush, ui::Point position) override { }
	void Draw(Simulation *sim, Brush const &brush, ui::Point position) override { };
	void DrawLine(Simulation * sim, Brush const &brush, ui::Point position1, ui::Point position2, bool dragging) override { };
	void DrawRect(Simulation * sim, Brush const &brush, ui::Point position1, ui::Point position2) override { };
	void DrawFill(Simulation * sim, Brush const &brush, ui::Point position) override { };

	void Select(int toolSelection) final override;
};
