#include "simulation/ToolCommon.h"

static int perform(SimTool *tool, Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);

void SimTool::Tool_AMBM()
{
	Identifier = "DEFAULT_TOOL_AMBM";
	Name = "AMBM";
	Colour = 0x00DDFF_rgb;
	Description = "Decreases ambient air temperature.";
	Perform = &perform;
}

static int perform(SimTool *tool, Simulation *sim, Particle *cpart, int x, int y, int brushX, int brushY, float strength)
{
	if (!sim->aheat_enable)
	{
		return 0;
	}

	sim->hv[{ x / CELL, y / CELL }] -= strength * 2.0f;
	if (sim->hv[{ x / CELL, y / CELL }] > MAX_TEMP) sim->hv[{ x / CELL, y / CELL }] = MAX_TEMP;
	if (sim->hv[{ x / CELL, y / CELL }] < MIN_TEMP) sim->hv[{ x / CELL, y / CELL }] = MIN_TEMP;
	return 1;
}
