#pragma once
#include "SimulationConfig.h"
#include "Pixel.h"
#include "common/Plane.h"
#include <vector>

using RendererFrame = PlaneAdapter<std::vector<pixel>>;

struct RendererStats
{
	int foundParticles = 0;
	float hdispLimitMin = 0;
	float hdispLimitMax = 0;
	bool hdispLimitValid = false;
};
