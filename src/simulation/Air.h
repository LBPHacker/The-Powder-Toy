#pragma once
#include "SimulationConfig.h"
#include "common/Plane.h"
#include <vector>

class Simulation;
struct RenderableSimulation;

class Air
{
public:
	Simulation & sim;
	int airMode;
	float ambientAirTemp;
	float vorticityCoeff;
	PlaneAdapter<std::vector<float>> ovx;
	PlaneAdapter<std::vector<float>> ovy;
	PlaneAdapter<std::vector<float>> opv;
	PlaneAdapter<std::vector<float>> ohv; // Ambient Heat
	PlaneAdapter<std::vector<unsigned char>> bmap_blockair;
	PlaneAdapter<std::vector<unsigned char>> bmap_blockairh;
	float kernel[9];
	void make_kernel(void);
	static float vorticity(const RenderableSimulation & sm, int y, int x);
	void update_airh(void);
	void update_air(void);
	void Clear();
	void ClearAirH();
	void Invert();
	void ApproximateBlockAirMaps();
	Air(Simulation & sim);
};
