#pragma once
#include "common/String.h"
#include "graphics/Pixel.h"

struct CustomGOLData
{
	int rule;
	RGB<uint8_t> colour1, colour2;
	String nameString;

	inline bool operator<(const CustomGOLData &other) const
	{
		return rule < other.rule;
	}
};
