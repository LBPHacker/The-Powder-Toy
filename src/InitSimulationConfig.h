#pragma once
#include "common/Vec2.h"
#include <stdexcept>

struct SimulationConfig
{
	int CELL;
	Vec2<int> CELLS;
	int NPART;

	struct CheckFailed : public std::runtime_error
	{
		using runtime_error::runtime_error;
	};
	void Check() const;

	static SimulationConfig Default()
	{
		return { 4, { 153, 96 }, 235008 };
	}

	bool CanSave() const;

	auto operator <=>(const SimulationConfig &other) const = default;
};

void InitSimulationConfig(SimulationConfig newConfig = {});
const SimulationConfig &GetCurrentSimulationConfig();
const SimulationConfig &GetNextSimulationConfig();
void SetNextSimulationConfig(SimulationConfig newConfig);
