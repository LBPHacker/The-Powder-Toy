#pragma once
#include "Particle.h"
#include "Sign.h"
#include "Stickman.h"
#include "common/tpt-rand.h"
#include "common/Plane.h"
#include <vector>
#include <array>
#include <json/json.h>

class Snapshot
{
public:
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> AirPressure;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> AirVelocityX;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> AirVelocityY;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> AmbientHeat;

	std::vector<Particle> Particles;

	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> GravVelocityX;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> GravVelocityY;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> GravValue;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> GravMap;

	PlaneAdapter<std::vector<unsigned char>, XCELLSExtent, YCELLSExtent> BlockMap;
	PlaneAdapter<std::vector<unsigned char>, XCELLSExtent, YCELLSExtent> ElecMap;
	PlaneAdapter<std::vector<unsigned char>, XCELLSExtent, YCELLSExtent> BlockAir;
	PlaneAdapter<std::vector<unsigned char>, XCELLSExtent, YCELLSExtent> BlockAirH;

	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> FanVelocityX;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> FanVelocityY;


	std::vector<Particle> PortalParticles;
	std::vector<int> WirelessData;
	std::vector<playerst> stickmen;
	std::vector<sign> signs;

	uint64_t FrameCount;
	RNG::State RngState;

	uint32_t Hash() const;

	Json::Value Authors;

	virtual ~Snapshot() = default;
};
