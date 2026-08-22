#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_SOIL()
{
	Identifier = "DEFAULT_PT_SOIL";
	Name = "SOIL";
	Colour = 0x9F7616_rgb;
	MenuVisible = 1;
	MenuSection = SC_ORGANIC;
	Enabled = 1;

	Advection = 0.7f;
	AirDrag = 0.02f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.95f;
	Collision = 0.0f;
	Gravity = 0.2f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 2;
	Hardness = 2;

	Weight = 55;

	HeatConduct = 70;
	Description = "Soil. Turns into mud when wet, great for growing seeds.";

	Properties = TYPE_PART;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 60.0f + 273.15f;
	HighTemperatureTransition = PT_CLST;

	Update = &update;
	Graphics = &graphics;

	Create = &create;
}

static int update(UPDATE_FUNC_ARGS)
{
	/**
	 * If tmp2 = 1 is a tunnel, don't move
	 * If tmp2 = 2 is a tunnel, allow particles
	 */

	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;

	// Permafrost or tunnel, dont move
	if (parts[i].temp < 273.15f || parts[i].tmp2 > 0)
	{
		parts[i].vx = parts[i].vy = 0.0f;
	}

	for (auto rx = -1; rx <= 1; rx++)
	{
		for (auto ry = -1; ry <= 1; ry++)
		{
			if (rx || ry)
			{
				auto r = pmap[y + ry][x + rx];
				if (!r)
				{
					continue;
				}
				auto rt = TYP(r);

				// Absorb water
				if (sim->rng.chance(1, 3))
				{
					if (rt == PT_SLTW && sim->rng.chance(1, 5))
					{
						sim->part_change_type(ID(r), x + rx, y + ry, PT_SALT);
						sim->part_change_type(i, x, y, PT_MUD);
						return 0;
					}
					else if (rt == PT_SWTR && sim->rng.chance(1, 5))
					{
						sim->part_change_type(ID(r), x + rx, y + ry, PT_SUGR);
						sim->part_change_type(i, x, y, PT_MUD);
						return 0;
					}
					else if (elements[rt].Properties & PROP_WATER)
					{
						sim->kill_part(ID(r));
						sim->part_change_type(i, x, y, PT_MUD);
						return 0;
					}
				}
			}
		}
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	auto z = (cpart->tmp - 5) * 8; // Speckles!
	*colr += z;
	*colg += z;
	*colb += z;

	// Turn blueish if cold
	if (cpart->temp < 273.15f)
	{
		*colg += (273.15f - cpart->temp) / 8;
		*colb += (273.15f - cpart->temp) / 3;
	}

	// Darken if part of tunnel
	if (cpart->tmp2 == 2)
	{
		*colr *= 0.6f;
		*colg *= 0.6f;
		*colb *= 0.6f;
	}
	return 0;
}

static void create(ELEMENT_CREATE_FUNC_ARGS)
{
	sim->parts[i].tmp = sim->rng.between(0, 6);
}
