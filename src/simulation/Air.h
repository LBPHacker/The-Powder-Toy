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
	PlaneAdapter<std::vector<unsigned char>, XCELLSExtent, YCELLSExtent> *bmap;
	PlaneAdapter<std::vector<unsigned char>, XCELLSExtent, YCELLSExtent> *emap;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> *fvx;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> *fvy;
	//
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> *vx{};
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> ovx;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> *vy{};
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> ovy;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> *pv{};
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> opv;
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> *hv{};
	PlaneAdapter<std::vector<float>, XCELLSExtent, YCELLSExtent> ohv; // Ambient Heat
	PlaneAdapter<std::vector<unsigned char>, XCELLSExtent, YCELLSExtent> bmap_blockair;
	PlaneAdapter<std::vector<unsigned char>, XCELLSExtent, YCELLSExtent> bmap_blockairh;
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
