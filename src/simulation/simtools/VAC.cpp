#include "simulation/ToolCommon.h"
#include "simulation/Air.h"

static int perform(SimTool *tool, Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);

void SimTool::Tool_VAC()
{
	Identifier = "DEFAULT_TOOL_VAC";
	Name = "VAC";
	Colour = 0x303030_rgb;
	Description = "Vacuum, reduces air pressure.";
	Perform = &perform;
}

static int perform(SimTool *tool, Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength)
{
	sim->pv[{ x/CELL, y/CELL }] -= strength*0.05f;

	if (sim->pv[{ x/CELL, y/CELL }] > MAX_PRESSURE)
		sim->pv[{ x/CELL, y/CELL }] = MAX_PRESSURE;
	else if (sim->pv[{ x/CELL, y/CELL }] < MIN_PRESSURE)
		sim->pv[{ x/CELL, y/CELL }] = MIN_PRESSURE;
	return 1;
}
