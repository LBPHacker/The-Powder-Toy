#include "WallTool.h"

#include "Misc.h"
#include "activities/game/Game.h"
#include "simulation/Simulation.h"
#include "graphics/Pix.h"
#include "graphics/CommonShapes.h"

namespace activities
{
namespace game
{
namespace tool
{
	void WallTool::Draw(gui::Point pos) const
	{
		auto *sim = Game::Ref().GetSimulation();
		sim->CreateWalls(pos.x / CELL * CELL, pos.y / CELL * CELL, 0, 0, wall, NULL);
	}

	void WallTool::Fill(gui::Point pos) const
	{
		auto *sim = Game::Ref().GetSimulation();
		// * TODO-REDO_UI: WL_FAN
		if (wall != WL_STREAM)
		{
			sim->FloodWalls(pos.x / CELL * CELL, pos.y / CELL * CELL, wall, -1);
		}
	}

	const String &WallTool::Name() const
	{
		auto &wl = Game::Ref().GetSimulation()->wtypes[wall];
		return wl.name;
	}

	const String &WallTool::Description() const
	{
		auto &wl = Game::Ref().GetSimulation()->wtypes[wall];
		return wl.descs;
	}

	gui::Color WallTool::Color() const
	{
		auto &wl = Game::Ref().GetSimulation()->wtypes[wall];
		return gui::Color{ PixR(wl.colour), PixG(wl.colour), PixB(wl.colour), 255 };
	}

	bool WallTool::MenuVisible() const
	{
		return true;
	}

	int WallTool::MenuSection() const
	{
		return SC_WALL;
	}

	int WallTool::MenuPrecedence() const
	{
		return wall;
	}

	void WallTool::GenerateTexture(gui::Point size, uint32_t *pixels) const
	{
		auto wgSize = gui::Point{ (size.x / CELL + 1) * CELL, (size.y / CELL + 1) * CELL };
		auto wallGraphics = [wgSize](bool powered, wall_type &wl) {
			std::vector<uint32_t> wgPixels(wgSize.x * wgSize.y, 0xFF000000);
			for (auto y = 0; y < wgSize.y / CELL; ++y)
			{
				for (auto x = 0; x < wgSize.x / CELL; ++x)
				{
					::Wall(x, y, powered, wl.drawstyle, wl.colour, wl.eglow, [&wgPixels, wgSize](int x, int y, int c) {
						wgPixels[y * wgSize.x + x] = c | 0xFF000000;
					});
				}
			}
			return wgPixels;
		};
		auto drawPixel = [pixels, size](int x, int y, int c) {
			pixels[y * size.x + x] = c;
		};
		switch (wall)
		{
		case WL_ERASE:
			{
				auto  left = wallGraphics(false, Game::Ref().GetSimulation()->wtypes[WL_DESTROYALL]);
				auto right = wallGraphics(false, Game::Ref().GetSimulation()->wtypes[WL_WALL]);
				for (auto y = 0; y < size.y; ++y)
				{
					for (auto x = 0; x < size.x; ++x)
					{
						if (x < 13)
						{
							pixels[y * size.x + x] = left[y * wgSize.x + x];
						}
						else
						{
							pixels[y * size.x + x] = right[y * wgSize.x + x];
						}
					}
				}
				RedCross(9, 3, drawPixel);
			}
			break;

		case WL_STREAM:
			{
				for (auto y = 0; y < size.y; ++y)
				{
					for (auto x = 0; x < size.x; ++x)
					{
						drawPixel(x, y, x == 0 || y == 0 || x == size.x - 1 || y == size.y - 1 ? 0xFF808080 : 0xFF000000);
					}
				}
				StreamBase(6, 6, drawPixel);
				static const int line[][2] = {
					{  6,  6 },
					{  7,  5 },
					{  8,  5 },
					{  9,  4 },
					{ 10,  4 },
					{ 11,  4 },
					{ 12,  4 },
					{ 13,  5 },
					{ 14,  6 },
					{ 15,  7 },
					{ 16,  7 },
					{ 17,  8 },
					{ 18,  9 },
					{ 19, 10 },
					{ 20, 10 },
					{ 21, 10 },
					{ 22, 10 },
					{ 23, 10 },
					{ 24,  9 },
				};
				for (auto &pixel : line)
				{
					drawPixel(pixel[0], pixel[1], 0xFFC0C0C0);
				}
			}
			break;

		case WL_ERASEALL:
			for (auto y = 0; y < size.y; ++y)
			{
				for (auto x = 0; x < size.x; ++x)
				{
					int r, g, b;
					HSV_to_RGB((420 - (x * 360 / size.x)) % 360, 180, 130, &r, &g, &b);
					pixels[y * size.x + x] = PixRGB(r, g, b);
				}
			}
			RedCross(3, 3, drawPixel);
			RedCross(14, 3, drawPixel);
			break;

		default:
			auto &wl = Game::Ref().GetSimulation()->wtypes[wall];
			auto unpowered = wallGraphics(false, wl);
			auto   powered = wallGraphics( true, wl);
			for (auto y = 0; y < size.y; ++y)
			{
				for (auto x = 0; x < size.x; ++x)
				{
					if (x - y > 5)
					{
						pixels[y * size.x + x] = powered[y * wgSize.x + x];
					}
					else
					{
						pixels[y * size.x + x] = unpowered[y * wgSize.x + x];
					}
				}
			}
			break;
		}
	}

	bool WallTool::EnablesGravityZones() const
	{
		return wall == WL_GRAV;
	}
}
}
}
