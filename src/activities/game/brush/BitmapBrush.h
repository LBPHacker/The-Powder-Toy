#pragma once

#include "Brush.h"

namespace activities
{
namespace game
{
namespace brush
{
	class BitmapBrush : public Brush
	{
		std::vector<unsigned char> data;
		gui::Point dataSize;

	public:
		BitmapBrush(const std::vector<unsigned char> &data, gui::Point dataSize);

		void GenerateBody() final override;
	};
}
}
}
