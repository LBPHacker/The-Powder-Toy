#include "simulation/ToolCommon.h"
#include "simulation/Air.h"

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);

void SimTool::Tool_AIR()
{
	Identifier = "DEFAULT_TOOL_AIR";
	Name = "AIR";
	Colour = 0xFFFFFF_rgb;
	Description = "Air, creates airflow and pressure.";
	Perform = &perform;
}

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength)
{
	sim->pv[{ x/CELL, y/CELL }] += strength*0.05f;

	if (sim->pv[{ x/CELL, y/CELL }] > MAX_PRESSURE)
		sim->pv[{ x/CELL, y/CELL }] = MAX_PRESSURE;
	else if (sim->pv[{ x/CELL, y/CELL }] < MIN_PRESSURE)
		sim->pv[{ x/CELL, y/CELL }] = MIN_PRESSURE;
	return 1;
}
