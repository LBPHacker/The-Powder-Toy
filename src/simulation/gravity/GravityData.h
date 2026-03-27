#pragma once
#include "common/Plane.h"
#include "common/AlignedAllocator.h"
#include "SimulationConfig.h"
#include <cstdint>
#include <vector>
#include <new>

template<class Item>
using GravityPlane = PlaneAdapter<std::vector<Item, AlignedAllocator<Item>>, XCELLSPERF, YCELLS>;

struct alignas(64) GravityInput
{
	GravityPlane<float> mass;
	GravityPlane<uint32_t> mask;
	static_assert(sizeof(float) == sizeof(uint32_t));

	GravityInput() : mass({ XCELLSPERF, YCELLS }, 0.f), mask({ XCELLSPERF, YCELLS }, UINT32_C(0xFFFFFFFF))
	{
	}
};

struct alignas(64) GravityOutput
{
	GravityPlane<float> forceX;
	GravityPlane<float> forceY;

	GravityOutput() : forceX({ XCELLSPERF, YCELLS }, 0.f), forceY({ XCELLSPERF, YCELLS }, 0.f)
	{
	}
};
