#pragma once
#include "SimulationConfig.h"
#include "common/Plane.h"
#include <vector>

class Simulation;

class Air
{
public:
	Simulation & sim;
	int airMode;
	float ambientAirTemp;
	//Arrays from the simulation
	PlaneAdapter<std::vector<unsigned char>> *bmap;
	PlaneAdapter<std::vector<unsigned char>> *emap;
	PlaneAdapter<std::vector<float>> *fvx;
	PlaneAdapter<std::vector<float>> *fvy;
	//
	PlaneAdapter<std::vector<float>> *vx{};
	PlaneAdapter<std::vector<float>> ovx;
	PlaneAdapter<std::vector<float>> *vy{};
	PlaneAdapter<std::vector<float>> ovy;
	PlaneAdapter<std::vector<float>> *pv{};
	PlaneAdapter<std::vector<float>> opv;
	PlaneAdapter<std::vector<float>> *hv{};
	PlaneAdapter<std::vector<float>> ohv; // Ambient Heat
	PlaneAdapter<std::vector<unsigned char>> bmap_blockair;
	PlaneAdapter<std::vector<unsigned char>> bmap_blockairh;
	float kernel[9];
	void make_kernel(void);
	void update_airh(void);
	void update_air(void);
	void Clear();
	void ClearAirH();
	void Invert();
	void ApproximateBlockAirMaps();
	Air(Simulation & sim);
};
