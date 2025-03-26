#pragma once
#include "common/String.h"
#include "common/Vec2.h"
#include "graphics/Pixel.h"
#include "graphics/VideoBuffer.h"
#include "gui/interface/Point.h"
#include "simulation/StructProperty.h"
#include "simulation/MenuSection.h"
#include "Lang/Translation.hpp"
#include <memory>
#include <optional>
#include <variant>

class Simulation;
class Brush;
struct Particle;

class Tool
{
public:
	std::unique_ptr<VideoBuffer> (*textureGen)(int, Vec2<int>) = nullptr;

	int ToolID = 0;
	String Name = "NULL";
	Powder::Lang::Translation Description;
	ByteString Identifier = "DEFAULT_TOOL_INVALID";
	RGB Colour = 0xFFFFFF_rgb;
	bool Blocky = false;
	float Strength = 1.0f;
	bool shiftBehaviour = false;
	bool ctrlBehaviour = false;
	bool altBehaviour = false;
	int MenuSection = SC_TOOL;
	bool MenuVisible = true;
	int MenuSort = 0;

	Tool() = default;

	Tool(int id, String name,
		RGB colour, ByteString identifier, std::unique_ptr<VideoBuffer> (*textureGen)(int, Vec2<int>) = nullptr, bool blocky = false
	):
		textureGen(textureGen),
		ToolID(id),
		Name(name),
		Identifier(identifier),
		Colour(colour),
		Blocky(blocky)
	{}

	Tool(int id, ByteString identifier) : ToolID(id), Identifier(identifier)
	{}

	virtual ~Tool()
	{}

	std::unique_ptr<VideoBuffer> GetTexture(Vec2<int>);
	virtual void Click(Simulation * sim, Brush const &brush, ui::Point position);
	virtual void Draw(Simulation * sim, Brush const &brush, ui::Point position);
	virtual void DrawLine(Simulation * sim, Brush const &brush, ui::Point position1, ui::Point position2, bool dragging);
	virtual void DrawRect(Simulation * sim, Brush const &brush, ui::Point position1, ui::Point position2);
	virtual void DrawFill(Simulation * sim, Brush const &brush, ui::Point position);

	virtual void Drag(Simulation *sim, const Brush &brush, ui::Point position1, ui::Point position2)
	{
	}

	virtual void Select(int toolSelection)
	{
	}
};
