#include "TriangleBrush.h"

namespace activities
{
namespace game
{
namespace brush
{
	void TriangleBrush::GenerateBody()
	{
		auto nx = 2 * effectiveRadius.y;
		auto ny = -effectiveRadius.x;
		for (auto y = -effectiveRadius.y; y <= effectiveRadius.y; ++y)
		{
			for (auto x = -effectiveRadius.x; x <= effectiveRadius.x; ++x)
			{
				auto xx = x;
				auto yy = y + effectiveRadius.y;
				if (xx < 0)
				{
					xx = -xx;
				}
				Body(gui::Point{ x, y }) = (xx * nx + yy * ny <= 0) ? 1 : 0;
			}
		}
	}
}
}
}

