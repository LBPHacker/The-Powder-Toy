#pragma once
#include "common/Plane.h"
#include "common/AlignedAllocator.h"
#include "SimulationConfig.h"
#include <cstdint>
#include <vector>
#include <new>

template<class Item>
using GravityPlane = PlaneAdapter<std::vector<Item, AlignedAllocator<Item>>, XCELLS_ALIGNED, YCELLS>;

struct alignas(64) GravityInput
{
	GravityPlane<float> mass;
	GravityPlane<uint32_t> mask;
	static_assert(sizeof(float) == sizeof(uint32_t));

	GravityInput() : mass({ XCELLS_ALIGNED, YCELLS }, 0.f), mask({ XCELLS_ALIGNED, YCELLS }, UINT32_C(0xFFFFFFFF))
	{
	}
};

struct alignas(64) GravityOutput
{
	GravityPlane<float> forceX;
	GravityPlane<float> forceY;

	GravityOutput() : forceX({ XCELLS_ALIGNED, YCELLS }, 0.f), forceY({ XCELLS_ALIGNED, YCELLS }, 0.f)
	{
	}
};
