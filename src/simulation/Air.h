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
	float ovx[YCELLS][XCELLS_ALIGNED];
	float ovy[YCELLS][XCELLS_ALIGNED];
	float opv[YCELLS][XCELLS_ALIGNED];
	float ohv[YCELLS][XCELLS_ALIGNED]; // Ambient Heat
	unsigned char bmap_blockair[YCELLS][XCELLS_ALIGNED];
	unsigned char bmap_blockairh[YCELLS][XCELLS_ALIGNED];
	static float vorticity(const RenderableSimulation & sm, int y, int x);
	void update_airh(void);
	void update_air(void);
	void Clear();
	void ClearAirH();
	void Invert();
	void ApproximateBlockAirMaps(Rect<int> targetBlocks);
	Air(Simulation & sim);
	void CopyFrom(const Air &other);
};
