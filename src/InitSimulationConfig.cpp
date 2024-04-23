#include "InitSimulationConfig.h"
#include "SimulationConfig.h"
#include "simulation/ElementDefs.h"
#include "common/String.h"
#include "prefs/GlobalPrefs.h"

int CELL;
int CELL3;
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

namespace
{
	struct FullSimulationConfig
	{
		int vCELL;
		int vCELL3;
		Vec2<int> vCELLS;
		Vec2<int> vRES;
		int vXCELLS;
		int vYCELLS;
		int vNCELL;
		int vXRES;
		int vYRES;
		int vNPART;
		int vXCNTR;
		int vYCNTR;
		Vec2<int> vWINDOW;
		int vWINDOWW;
		int vWINDOWH;

		FullSimulationConfig() = default;

		FullSimulationConfig(SimulationConfig config)
		{
			vCELL    = config.vCELL;
			vCELL3   = vCELL * 3;
			vCELLS   = config.vCELLS;
			vRES     = vCELLS * vCELL;
			vXCELLS  = vCELLS.X;
			vYCELLS  = vCELLS.Y;
			vNCELL   = vXCELLS * vYCELLS;
			vXRES    = vRES.X;
			vYRES    = vRES.Y;
			vNPART   = std::min(vXRES * vYRES, 1 << (31 - PMAPBITS));
			vXCNTR   = vXRES / 2;
			vYCNTR   = vYRES / 2;
			vWINDOW  = vRES + Vec2(BARSIZE, MENUSIZE);
			vWINDOWW = vWINDOW.X;
			vWINDOWH = vWINDOW.Y;
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
	CELL    = full.vCELL;
	CELLS   = full.vCELLS;
	RES     = full.vRES;
	XCELLS  = full.vXCELLS;
	YCELLS  = full.vYCELLS;
	NCELL   = full.vNCELL;
	XRES    = full.vXRES;
	YRES    = full.vYRES;
	NPART   = full.vNPART;
	XCNTR   = full.vXCNTR;
	YCNTR   = full.vYCNTR;
	WINDOW  = full.vWINDOW;
	WINDOWW = full.vWINDOWW;
	WINDOWH = full.vWINDOWH;
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
	checkBounds("cell size"            , vCELL   , 1,   100);
	checkBounds("horizontal cell count", vCELLS.X, 1, 15000);
	checkBounds("vertical cell count"  , vCELLS.Y, 1, 15000);
	auto full = FullSimulationConfig(*this);
	checkBounds("simulation width" , full.vRES.X, 1, 15000);
	checkBounds("simulation height", full.vRES.Y, 1, 15000);
}

bool SimulationConfig::operator ==(const SimulationConfig &other) const
{
	return std::tie(vCELL, vCELLS) == std::tie(other.vCELL, other.vCELLS);
}

bool SimulationConfig::CanSave() const
{
	return vCELLS.X <= 255 && vCELLS.Y <= 255;
}
