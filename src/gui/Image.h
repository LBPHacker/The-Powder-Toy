#pragma once

#include "Point.h"

#include <cstdint>
#include <vector>

namespace gui
{
	struct Image
	{
		Point size;
		std::vector<uint32_t> pixels;
	};
}
