#pragma once
#include "SimulationConfig.h"

class Simulation;
struct RenderableSimulation;

class Air
{
public:
	Simulation & sim;
	int airMode;
	float ambientAirTemp;
	float edgePressure;
	float edgeVelocityX;
	float edgeVelocityY;
	float vorticityCoeff;
	int convectionMode;
	float ovx[YCELLS][XCELLSPERF];
	float ovy[YCELLS][XCELLSPERF];
	float opv[YCELLS][XCELLSPERF];
	float ohv[YCELLS][XCELLSPERF]; // Ambient Heat
	unsigned char bmap_blockair[YCELLS][XCELLSPERF];
	unsigned char bmap_blockairh[YCELLS][XCELLSPERF];
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
