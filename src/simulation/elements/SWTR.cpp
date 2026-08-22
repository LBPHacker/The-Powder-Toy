#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_SWTR()
{
	Identifier = "DEFAULT_PT_SWTR";
	Name = "SWTR";
	Colour = 0x5362F5_rgb;
	MenuVisible = 1;
	MenuSection = SC_ORGANIC;
	Enabled = 1;

	Advection = 0.6f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.98f;
	Loss = 0.95f;
	Collision = 0.0f;
	Gravity = 0.1f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 2;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 35;

	HeatConduct = 75;
	Description = "Sugar water.";

	Properties = TYPE_LIQUID | PROP_CONDUCTS | PROP_PHOTPASS | PROP_LIFE_DEC | PROP_NEUTPENETRATE | PROP_WATER;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = -21.1f + 273.15f;
	LowTemperatureTransition = PT_ICEI;
	HighTemperature = 109.85f + 273.15f;
	HighTemperatureTransition = ST;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS)
{
	for (auto rx = -1; rx <= 1; rx++)
	{
		for (auto ry = -1; ry <= 1; ry++)
		{
			if (rx || ry)
			{
				auto r = pmap[y + ry][x + rx];
				switch (TYP(r))
				{
					case PT_SUGR: // Grow sugar crystals
						if (sim->rng.chance(1, 2000))
						{
							sim->part_change_type(i, x, y, PT_SUGR);
							return 1;
						}
						break;

					case PT_PLNT:
						if (sim->rng.chance(1, 2000))
						{
							sim->part_change_type(i, x, y, PT_PLNT);
							return 1;
						}
						break;

					case PT_RBDM:
					case PT_LRBD:
						if ((sim->legacy_enable || parts[i].temp > 12.0f + 273.15f) && sim->rng.chance(1, 100))
						{
							sim->part_change_type(i, x, y, PT_FIRE);
							parts[i].life = 4;
							parts[i].ctype = PT_WATR;
							return 1;
						}
						break;

					case PT_FIRE:
						if (parts[ID(r)].ctype != PT_WATR)
						{
							sim->kill_part(ID(r));
							if (sim->rng.chance(1, 30))
							{
								sim->kill_part(i);
								return 1;
							}
						}
						break;

					case PT_YEST:
						if (sim->rng.chance(1, 300))
						{
							sim->part_change_type(i, x, y, PT_YEST);
							return 1;
						}
				}
			}
		}
	}

	return 0;
}
