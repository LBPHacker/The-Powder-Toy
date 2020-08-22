#pragma once

#include "gui/Point.h"

#include <array>
#include <vector>

namespace activities
{
namespace game
{
namespace tool
{
	class Tool;
}

namespace brush
{
	class Brush
	{
	protected:
		gui::Point requestedRadius, effectiveRadius;
		std::vector<unsigned char> body;
		std::vector<unsigned char> outline;

		std::array<std::vector<gui::Point>, 4> offsetLists;

		virtual void GenerateBody() = 0;
		void GenerateOutline();
		void GenerateOffsetLists();
		void Generate();

		virtual void UpdateEffectiveRadius()
		{
			effectiveRadius = requestedRadius;
		}

	public:
		virtual ~Brush() = default;

		void RequestedRadius(gui::Point newRequestedRadius);
		gui::Point RequestedRadius() const
		{
			return requestedRadius;
		}

		gui::Point EffectiveRadius() const
		{
			return effectiveRadius;
		}

		gui::Point EffectiveSize() const
		{
			return 2 * effectiveRadius + gui::Point{ 1, 1 };
		}

		unsigned char &Body(gui::Point p)
		{
			return body[(p.y + effectiveRadius.y) * (effectiveRadius.x * 2 + 1) + (p.x + effectiveRadius.x)];
		}
		const unsigned char &Body(gui::Point p) const
		{
			return body[(p.y + effectiveRadius.y) * (effectiveRadius.x * 2 + 1) + (p.x + effectiveRadius.x)];
		}

		unsigned char &Outline(gui::Point p)
		{
			return outline[(p.y + effectiveRadius.y) * (effectiveRadius.x * 2 + 1) + (p.x + effectiveRadius.x)];
		}
		const unsigned char &Outline(gui::Point p) const
		{
			return outline[(p.y + effectiveRadius.y) * (effectiveRadius.x * 2 + 1) + (p.x + effectiveRadius.x)];
		}

		std::vector<gui::Point> &OffsetList(int index)
		{
			return offsetLists[index];
		}
		const std::vector<gui::Point> &OffsetList(int index) const
		{
			return offsetLists[index];
		}

		void Draw(tool::Tool *tool, gui::Point from, gui::Point to) const;

		virtual bool CellAligned() const
		{
			return false;
		}
	};
}
}
}
