#pragma once
#include "common/Plane.h"
#include "common/AlignedAllocator.h"
#include "SimulationConfig.h"
#include <cstdint>
#include <vector>

template<class Item>
using GravityPlane = PlaneAdapter<std::vector<Item, AlignedAllocator<Item>>, CELLS_ALIGNED.X, CELLS_ALIGNED.Y>;

struct GravityInput
{
	GravityPlane<float> mass;
	GravityPlane<uint32_t> mask;
	static_assert(sizeof(float) == sizeof(uint32_t));

	GravityInput() : mass(CELLS_ALIGNED, 0.f), mask(CELLS_ALIGNED, UINT32_C(0xFFFFFFFF))
	{
	}
};

struct GravityOutput
{
	GravityPlane<float> forceX;
	GravityPlane<float> forceY;

	GravityOutput() : forceX(CELLS_ALIGNED, 0.f), forceY(CELLS_ALIGNED, 0.f)
	{
	}
};
