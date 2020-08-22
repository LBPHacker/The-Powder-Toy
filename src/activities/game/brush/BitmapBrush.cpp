#include "BitmapBrush.h"

#include <cmath>

namespace activities
{
namespace game
{
namespace brush
{
	BitmapBrush::BitmapBrush(const std::vector<unsigned char> &data, gui::Point dataSize) :
		data(data),
		dataSize(dataSize)
	{
		RequestedRadius(gui::Point{ 0, 0 });
	}

	void BitmapBrush::GenerateBody()
	{
		auto data2D = [this](int x, int y) -> unsigned char {
			// * The bound check is not required in theory, as it's guaranteed
			//   that whatever value is read from beyond the end of data will
			//   ultimately not contribute to the average (due to being scaled
			//   by 0). In practice, though, out-of-bounds reads are evil.
			if (x < 0 || y < 0 || x >= dataSize.x || y >= dataSize.y)
			{
				return 0;
			}
			return data[y * dataSize.x + x];
		};
		auto sx = 2 * effectiveRadius.x / float(dataSize.x - 1);
		auto sy = 2 * effectiveRadius.y / float(dataSize.y - 1);
		for (auto y = -effectiveRadius.y; y <= effectiveRadius.y; ++y)
		{
			for (auto x = -effectiveRadius.x; x <= effectiveRadius.x; ++x)
			{
				float ix, iy;
				auto rx1 = modff((x + effectiveRadius.x) / sx, &ix);
				auto ry1 = modff((y + effectiveRadius.y) / sy, &iy);
				auto xx0 = int(ix);
				auto yy0 = int(iy);
				auto xx1 = xx0 + 1;
				auto yy1 = yy0 + 1;
				auto rx0 = 1.f - rx1;
				auto ry0 = 1.f - ry1;
				Body(gui::Point{ x, y }) =
					data2D(xx0, yy0) * rx0 * ry0 +
					data2D(xx1, yy0) * rx1 * ry0 +
					data2D(xx0, yy1) * rx0 * ry1 +
					data2D(xx1, yy1) * rx1 * ry1 >= 128 ? 1 : 0;
			}
		}
	}
}
}
}
