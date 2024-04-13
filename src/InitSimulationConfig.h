#pragma once
#include "common/Vec2.h"
#include <stdexcept>

struct SimulationConfig
{
	int CELL;
	Vec2<int> CELLS;
	int NPART;
	float MAX_TEMP;
	float MIN_TEMP;
	float MAX_PRESSURE;
	float MIN_PRESSURE;

	struct CheckFailed : public std::runtime_error
	{
		using runtime_error::runtime_error;
	};
	void Check() const;

	static SimulationConfig Default()
	{
		return { 4, { 153, 96 }, 235008, 9999.f, 0.f, 256.f, -256.f };
	}

	bool CanSave() const;

	bool operator ==(const SimulationConfig &other) const = default;
};

void InitSimulationConfig(SimulationConfig newConfig = {});
const SimulationConfig &GetCurrentSimulationConfig();
const SimulationConfig &GetNextSimulationConfig();
void SetNextSimulationConfig(SimulationConfig newConfig);
