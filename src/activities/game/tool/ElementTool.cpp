#include "ElementTool.h"

#include "activities/game/Game.h"
#include "simulation/Simulation.h"
#include "simulation/ElementClasses.h"
#include "graphics/Pix.h"
#include "graphics/CommonShapes.h"

namespace activities
{
namespace game
{
namespace tool
{
	void ElementTool::Draw(gui::Point pos) const
	{
		auto *sim = Game::Ref().GetSimulation();
		sim->CreateParts(pos.x, pos.y, 0, 0, elem, 0);
	}

	void ElementTool::Fill(gui::Point pos) const
	{
		auto *sim = Game::Ref().GetSimulation();
		sim->FloodParts(pos.x, pos.y, elem, -1, 0);
	}

	const String &ElementTool::Name() const
	{
		auto &el = Game::Ref().GetSimulation()->elements[TYP(elem)];
		return el.Name;
	}

	const String &ElementTool::Description() const
	{
		auto &el = Game::Ref().GetSimulation()->elements[TYP(elem)];
		return el.Description;
	}

	gui::Color ElementTool::Color() const
	{
		auto &el = Game::Ref().GetSimulation()->elements[TYP(elem)];
		return gui::Color{ PixR(el.Colour), PixG(el.Colour), PixB(el.Colour), 255 };
	}

	bool ElementTool::MenuVisible() const
	{
		auto &el = Game::Ref().GetSimulation()->elements[TYP(elem)];
		return el.MenuVisible;
	}

	int ElementTool::MenuSection() const
	{
		auto &el = Game::Ref().GetSimulation()->elements[TYP(elem)];
		return el.MenuSection;
	}

	int ElementTool::MenuPrecedence() const
	{
		return elem;
	}

	bool ElementTool::CanGenerateTexture() const
	{
		return TYP(elem) == PT_NONE;
	}

	void ElementTool::GenerateTexture(gui::Point size, uint32_t *pixels) const
	{
		auto drawPixel = [pixels, size](int x, int y, int c) {
			pixels[y * size.x + x] = c;
		};
		switch (TYP(elem))
		{
		case PT_NONE:
			for (auto y = 0; y < size.y; ++y)
			{
				for (auto x = 0; x < size.x; ++x)
				{
					pixels[y * size.x + x] = 0xFF000000;
				}
			}
			RedCross(9, 3, drawPixel);
			break;
		}
	}
}
}
}
