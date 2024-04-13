#include "InitSimulationConfig.h"
#include "SimulationConfig.h"
#include "simulation/ElementDefs.h"
#include "common/String.h"
#include "prefs/GlobalPrefs.h"

int CELL;
Vec2<int> CELLS;
Vec2<int> RES;
int XCELLS;
int YCELLS;
int NCELL;
int XRES;
int YRES;
int NPART;
int XCNTR;
int YCNTR;
Vec2<int> WINDOW;
int WINDOWW;
int WINDOWH;
int ISTP;
float CFDS;

namespace
{
	struct FullSimulationConfig
	{
		int CELL;
		Vec2<int> CELLS;
		Vec2<int> RES;
		int XCELLS;
		int YCELLS;
		int NCELL;
		int XRES;
		int YRES;
		int NPART;
		int XCNTR;
		int YCNTR;
		Vec2<int> WINDOW;
		int WINDOWW;
		int WINDOWH;
		int ISTP;
		float CFDS;

		FullSimulationConfig() = default;

		FullSimulationConfig(SimulationConfig config)
		{
			CELL    = config.CELL;
			CELLS   = config.CELLS;
			RES     = CELLS * CELL;
			XCELLS  = CELLS.X;
			YCELLS  = CELLS.Y;
			NCELL   = XCELLS * YCELLS;
			XRES    = RES.X;
			YRES    = RES.Y;
			NPART   = std::min(XRES * YRES, 1 << (31 - PMAPBITS));
			XCNTR   = XRES / 2;
			YCNTR   = YRES / 2;
			WINDOW  = RES + Vec2(BARSIZE, MENUSIZE);
			WINDOWW = WINDOW.X;
			WINDOWH = WINDOW.Y;
			ISTP    = CELL / 2;
			CFDS    = 4.0f / CELL;
			if (ISTP == 0)
			{
				ISTP = 1;
			}
		}
	};
}

namespace
{
	SimulationConfig currentConfig;
	SimulationConfig nextConfig;
}

void InitSimulationConfig(SimulationConfig newConfig)
{
	currentConfig = newConfig;
	nextConfig = currentConfig;
	auto full = FullSimulationConfig(currentConfig);
	CELL    = full.CELL;
	CELLS   = full.CELLS;
	RES     = full.RES;
	XCELLS  = full.XCELLS;
	YCELLS  = full.YCELLS;
	NCELL   = full.NCELL;
	XRES    = full.XRES;
	YRES    = full.YRES;
	NPART   = full.NPART;
	XCNTR   = full.XCNTR;
	YCNTR   = full.YCNTR;
	WINDOW  = full.WINDOW;
	WINDOWW = full.WINDOWW;
	WINDOWH = full.WINDOWH;
	ISTP    = full.ISTP;
	CFDS    = full.CFDS;
}

const SimulationConfig &GetCurrentSimulationConfig()
{
	return currentConfig;
}

const SimulationConfig &GetNextSimulationConfig()
{
	return nextConfig;
}

void SetNextSimulationConfig(SimulationConfig newConfig)
{
	nextConfig = newConfig;
}

void SimulationConfig::Check() const
{
	auto checkBounds = [](const char *name, auto value, auto min, auto max) {
		if (!(value >= min && value <= max))
		{
			throw CheckFailed(ByteString::Build(name, " is ", value, ", expected to be between ", min, " and ", max));
		}
	};
	checkBounds("cell size"            , CELL   , 1,   100);
	checkBounds("horizontal cell count", CELLS.X, 1, 15000);
	checkBounds("vertical cell count"  , CELLS.Y, 1, 15000);
	auto full = FullSimulationConfig(*this);
	checkBounds("simulation width" , full.RES.X, 300, 15000);
	checkBounds("simulation height", full.RES.Y, 60, 15000);
}

bool SimulationConfig::operator ==(const SimulationConfig &other) const
{
	return std::tie(CELL, CELLS) == std::tie(other.CELL, other.CELLS);
}

bool SimulationConfig::CanSave() const
{
	return CELLS.X <= 255 && CELLS.Y <= 255;
}
