#pragma once

#include "gui/Point.h"
#include "gui/Rect.h"
#include "gui/Color.h"
#include "common/String.h"

class Simulation;

namespace activities
{
namespace game
{
namespace brush
{
	class Brush;
}

namespace tool
{
	class Tool
	{
	public:
		virtual ~Tool() = default;

		virtual void Draw(gui::Point pos) const = 0;
		virtual void Fill(gui::Point pos) const = 0;
		virtual const String &Name() const = 0;
		virtual const String &Description() const = 0;
		virtual gui::Color Color() const = 0;
		virtual bool MenuVisible() const = 0;
		virtual int MenuSection() const = 0;
		virtual int MenuPrecedence() const = 0;

		virtual bool OptimizeLines() const
		{
			return false;
		}

		virtual bool AllowReplace() const
		{
			return false;
		}

		virtual bool CanGenerateTexture() const
		{
			return false;
		}

		virtual void GenerateTexture(gui::Point size, uint32_t *pixels) const
		{
		}

		virtual bool CellAligned() const
		{
			return false;
		}

		virtual bool EnablesGravityZones() const
		{
			return false;
		}
	};
}
}
}
