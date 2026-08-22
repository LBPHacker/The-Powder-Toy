#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_SUGR()
{
	Identifier = "DEFAULT_PT_SUGR";
	Name = "SUGR";
	Colour = 0xFFF9F2_rgb;
	MenuVisible = 1;
	MenuSection = SC_ORGANIC;
	Enabled = 1;

	Advection = 0.4f;
	AirDrag = 0.04f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.95f;
	Collision = -0.1f;
	Gravity = 0.3f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 20;
	Explosive = 1;
	Meltable = 2;
	Hardness = 2;

	Weight = 100;

	HeatConduct = 150;
	Description = "Sugar. Great food for bacteria.";

	Properties = TYPE_PART | PROP_NEUTPASS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 186.0f + 273.15f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS)
{
	auto r = sim->photons[y][x];

	// React with NEUT to make carbon, hydrogen, oxygen
	if (TYP(r) == PT_NEUT && sim->rng.chance(1, 20))
	{
		sim->kill_part(ID(r));
		if (sim->rng.chance(1, 3))
		{
			sim->part_change_type(i, x, y, PT_CRBN);
		}
		else
		{
			sim->part_change_type(i, x, y, sim->rng.chance(1, 2) ? PT_O2 : PT_H2);
		}
		return 1;
	}
	else
	{
		for (auto rx = -1; rx <= 1; rx++)
		{
			for (auto ry = -1; ry <= 1; ry++)
			{
				if (rx || ry)
				{
					r = pmap[y + ry][x + rx];
					if (!r)
					{
						continue;
					}

					if (TYP(r) == PT_WATR || TYP(r) == PT_DSTW) // Dissolve
					{
						sim->part_change_type(ID(r), x + rx, y + ry, PT_SWTR);
						if (sim->rng.chance(1, 2))
						{
							sim->kill_part(i);
							return 1;
						}
					}
					else if (TYP(r) == PT_YEST && sim->rng.chance(1, 300)) // Grow YEST
					{
						sim->part_change_type(i, x, y, PT_YEST);
						return 1;
					}
					else if (TYP(r) == PT_ACID || TYP(r) == PT_CAUS) // React with acid to form carbon
					{
						parts[i].temp += 120.0f;
						sim->part_change_type(ID(r), x + rx, y + ry, sim->rng.chance(1, 2) ? PT_CRBN : PT_WTRV);
						if (sim->rng.chance(1, 2))
						{
							sim->kill_part(i);
							return 1;
						}
					}
				}
			}
		}
	}
	return 0;
}
