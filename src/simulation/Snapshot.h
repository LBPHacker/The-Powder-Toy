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
	PlaneAdapter<std::vector<float>> AirPressure;
	PlaneAdapter<std::vector<float>> AirVelocityX;
	PlaneAdapter<std::vector<float>> AirVelocityY;
	PlaneAdapter<std::vector<float>> AmbientHeat;

	std::vector<Particle> Particles;

	PlaneAdapter<std::vector<float>> GravVelocityX;
	PlaneAdapter<std::vector<float>> GravVelocityY;
	PlaneAdapter<std::vector<float>> GravValue;
	PlaneAdapter<std::vector<float>> GravMap;

	PlaneAdapter<std::vector<unsigned char>> BlockMap;
	PlaneAdapter<std::vector<unsigned char>> ElecMap;
	PlaneAdapter<std::vector<unsigned char>> BlockAir;
	PlaneAdapter<std::vector<unsigned char>> BlockAirH;

	PlaneAdapter<std::vector<float>> FanVelocityX;
	PlaneAdapter<std::vector<float>> FanVelocityY;


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
