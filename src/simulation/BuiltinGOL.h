#pragma once

#include "common/String.h"
#include "graphics/Pixel.h"

struct BuiltinGOL
{
	String name;
	int oldtype;
	int ruleset;
	RGB<uint8_t> colour = RGB<uint8_t>(0, 0, 0);
	RGB<uint8_t> colour2 = RGB<uint8_t>(0, 0, 0);
	int goltype;
	String description;
};
