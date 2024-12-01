#pragma once
#include "Tool.h"

class WallTool : public Tool
{
public:
	WallTool(
		int id,
		String description,
		RGB<uint8_t> colour,
		ByteString identifier,
		std::unique_ptr<VideoBuffer> (*textureGen)(int, Vec2<int>) = NULL
	) :
		Tool(id, "", description, colour, identifier, textureGen, true)
	{
	}

	void Draw(Simulation *sim, const Brush &brush, ui::Point position) override;
	void
		DrawLine(Simulation *sim, const Brush &brush, ui::Point position1, ui::Point position2, bool dragging) override;
	void DrawRect(Simulation *sim, const Brush &brush, ui::Point position1, ui::Point position2) override;
	void DrawFill(Simulation *sim, const Brush &brush, ui::Point position) override;
};
