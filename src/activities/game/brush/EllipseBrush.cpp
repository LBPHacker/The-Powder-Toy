#include "EllipseBrush.h"

#include <cstdint>

namespace activities
{
namespace game
{
namespace brush
{
	void EllipseBrush::GenerateBody()
	{
		auto rr = int64_t(4) * effectiveRadius.x * effectiveRadius.x * effectiveRadius.y * effectiveRadius.y;
		auto sx = int64_t(2) * effectiveRadius.x;
		if (perfect && sx)
		{
			sx -= 1;
		}
		auto sxx = sx * sx;
		auto sy = int64_t(2) * effectiveRadius.y;
		if (perfect && sy)
		{
			sy -= 1;
		}
		auto syy = sy * sy;
		for (auto y = -effectiveRadius.y; y <= effectiveRadius.y; ++y)
		{
			for (auto x = -effectiveRadius.x; x <= effectiveRadius.x; ++x)
			{
				Body(gui::Point{ x, y }) = ((x * x) * syy + (y * y) * sxx <= rr) ? 1 : 0;
			}
		}
	}

	void EllipseBrush::Perfect(bool newPerfect)
	{
		perfect = newPerfect;
		Generate();
	}
}
}
}
