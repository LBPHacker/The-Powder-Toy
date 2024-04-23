#include "InitSimulationConfig.h"
#include "SimulationConfig.h"
#include "simulation/ElementDefs.h"
#include "common/String.h"
#include "prefs/GlobalPrefs.h"

#define GEN_LOCAL(t, n) static t l ## n;
SIM_PARAMS(GEN_LOCAL)
#undef GEN_LOCAL

#define DEF_GETTER(t, n) t n ## _Getter() { return l ## n; }
SIM_PARAMS(DEF_GETTER)
#undef DEF_GETTER

namespace
{
	struct FullSimulationConfig
	{
#define GEN_MEMBER(t, n) t v ## n;
SIM_PARAMS(GEN_MEMBER)
#undef GEN_MEMBER

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
#define SET_LOCAL(t, n) l ## n = full.v ## n;
SIM_PARAMS(SET_LOCAL)
#undef SET_LOCAL
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
