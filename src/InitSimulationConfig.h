#pragma once
#include "common/Vec2.h"
#include <stdexcept>

struct SimulationConfig
{
	int vCELL;
	Vec2<int> vCELLS;

	struct CheckFailed : public std::runtime_error
	{
		using runtime_error::runtime_error;
	};
	void Check() const;

	static SimulationConfig Default()
	{
		return { 4, { 153, 96 } };
	}

	bool CanSave() const;

	bool operator ==(const SimulationConfig &other) const;
};

void InitSimulationConfig(SimulationConfig newConfig = {});
const SimulationConfig &GetCurrentSimulationConfig();
const SimulationConfig &GetNextSimulationConfig();
void SetNextSimulationConfig(SimulationConfig newConfig);
