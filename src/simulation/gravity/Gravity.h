#pragma once
#include "Config.h"
#include "GravityPtr.h"
#include "common/Plane.h"
#include "SimulationConfig.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>
#include <cstdint>

class Simulation;

class Gravity
{
protected:
	bool enabled = false;

	// Maps to be processed by the gravity thread
	PlaneAdapter<std::vector<float>> th_ogravmap;
	PlaneAdapter<std::vector<float>> th_gravmap;
	PlaneAdapter<std::vector<float>> th_gravx;
	PlaneAdapter<std::vector<float>> th_gravy;
	PlaneAdapter<std::vector<float>> th_gravp;

	int th_gravchanged = 0;

	std::thread gravthread;
	std::mutex gravmutex;
	std::condition_variable gravcv;
	int grav_ready = 0;
	int gravthread_done = 0;
	bool ignoreNextResult = false;

	struct mask_el {
		PlaneAdapter<std::vector<char>> shape;
		char shapeout;
		mask_el *next;
	};
	using mask_el = struct mask_el;

	bool grav_mask_r(int x, int y, PlaneAdapter<std::vector<char>> &checkmap, PlaneAdapter<std::vector<char>> &shape);
	void mask_free(mask_el *c_mask_el);

	void update_grav();
	void get_result();
	void update_grav_async();
	
	struct CtorTag // Please use Gravity::Create().
	{
	};

public:
	Gravity(CtorTag);
	~Gravity();

	//Maps to be used by the main thread
	PlaneAdapter<std::vector<float>> *gravmap{};
	PlaneAdapter<std::vector<float>> *gravp{};
	PlaneAdapter<std::vector<float>> *gravy{};
	PlaneAdapter<std::vector<float>> *gravx{};
	PlaneAdapter<std::vector<uint32_t>> gravmask;
	static_assert(sizeof(float) == sizeof(uint32_t));

	PlaneAdapter<std::vector<unsigned char>> *bmap;

	bool IsEnabled() { return enabled; }

	void Clear();

	void gravity_update_async();

	void start_grav_async();
	void stop_grav_async();
	void gravity_mask();

	static GravityPtr Create();
};
