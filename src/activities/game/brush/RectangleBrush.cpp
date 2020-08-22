#include "RectangleBrush.h"

namespace activities
{
namespace game
{
namespace brush
{
	void RectangleBrush::GenerateBody()
	{
		for (auto y = -effectiveRadius.y; y <= effectiveRadius.y; ++y)
		{
			for (auto x = -effectiveRadius.x; x <= effectiveRadius.x; ++x)
			{
				Body(gui::Point{ x, y }) = 1;
			}
		}
	}
}
}
}
