#pragma once

#include "Tool.h"

namespace activities
{
namespace game
{
namespace tool
{
	class WallTool : public Tool
	{
		int wall;

	public:
		WallTool(int wall) : wall(wall)
		{
		}

		void Draw(gui::Point pos) const final override;
		void Fill(gui::Point pos) const final override;
		const String &Name() const final override;
		const String &Description() const final override;
		gui::Color Color() const final override;
		bool MenuVisible() const final override;
		int MenuSection() const final override;
		int MenuPrecedence() const final override;

		int Wall() const
		{
			return wall;
		}

		bool OptimizeLines() const final override
		{
			return true;
		}

		bool CanGenerateTexture() const final override
		{
			return true;
		}

		void GenerateTexture(gui::Point size, uint32_t *pixels) const final override;

		bool CellAligned() const final override
		{
			return true;
		}

		bool EnablesGravityZones() const final override;
	};
}
}
}
