#pragma once
#include <memory>
#include "Tool.h"
#include "graphics/Graphics.h"
#include "graphics/RendererFrame.h"

namespace Powder::Activity
{
	class Game;
}

class Renderer;
class GameView;
class DecorationTool: public Tool
{
public:
	Powder::Activity::Game &game;

	std::unique_ptr<VideoBuffer> GetIcon(int toolID, Vec2<int> size);

	DecorationTool(Powder::Activity::Game &newGame, int decoMode, String name, String description, RGB colour, ByteString identifier):
		Tool(decoMode, name, description, colour, identifier),
		game(newGame)
	{
		MenuSection = SC_DECO;
	}

	void Draw(Simulation * sim, Brush const &brush, ui::Point position) override;
	void DrawLine(Simulation * sim, Brush const &brush, ui::Point position1, ui::Point position2, bool dragging) override;
	void DrawRect(Simulation * sim, Brush const &brush, ui::Point position1, ui::Point position2) override;
	void DrawFill(Simulation * sim, Brush const &brush, ui::Point position) override;
};
