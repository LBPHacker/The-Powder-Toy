#pragma once
#include "SimulationConfig.h"
#include "Pixel.h"
#include "common/Plane.h"
#include <vector>

using RendererFrame = PlaneAdapter<std::vector<pixel>>;
