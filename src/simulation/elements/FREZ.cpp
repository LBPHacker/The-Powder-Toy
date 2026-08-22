#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_FREZ()
{
	Identifier = "DEFAULT_PT_FREZ";
	Name = "FREZ";
	Colour = 0xDEF9FC_rgb;
	MenuVisible = 1;
	MenuSection = SC_GAS;
	Enabled = 1;

	Advection = 1.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.99f;
	Loss = 0.30f;
	Collision = -0.1f;
	Gravity = 0.5f;
	Diffusion = 1.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 1;

	HeatConduct = 0;
	Description = "Frezon. (OP Feron) Super cold gas, does not gain heat energy";

	Properties = TYPE_GAS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;

	DefaultProperties.temp = 0.0f;
}

static int update(UPDATE_FUNC_ARGS)
{
	for (auto rx = -1; rx < 2; rx++)
	{
		for (auto ry = -1; ry < 2; ry++)
		{
			if (rx || ry)
			{
				auto r = pmap[y + ry][x + rx];
				if (!r)
				{
					r = sim->photons[y + ry][x + rx];
				}
				if (!r)
				{
					continue;
				}

				if (TYP(r) != PT_FREZ && TYP(r) != PT_TPRS)
				{
					parts[ID(r)].temp -= 25.0f;
				}

				// Put out fires
				if (TYP(r) == PT_FIRE || TYP(r) == PT_PLSM || TYP(r) == PT_DFLM)
				{
					sim->kill_part(ID(r));
					if (sim->rng.chance(1, 100))
					{
						sim->kill_part(i);
						return 0;
					}
				}

				// Detonate C5
				if (TYP(r) == PT_C5)
				{
					parts[i].life = 100;
					parts[ID(r)].temp = 0.0f;
					sim->part_change_type(i, x, y, PT_CFLM);
				}
			}
		}
	}

	// FREZ fusion. Occurs at high pressures
	float temp = parts[i].temp;
	if (temp < 100.0f && sim->pv[y / CELL][x / CELL] > 200.0f)
	{
		if (sim->rng.chance(1, 5))
		{
			sim->create_part(i, x, y, PT_PLSM);
			parts[i].tmp = 0x1;

			auto j = sim->create_part(-3, x + sim->rng.between(-1, 1), y + sim->rng.between(-1, 1), PT_NEUT);
			if (j >= 0)
			{
				parts[j].temp = temp;
			}
			if (sim->rng.chance(1, 10))
			{
				j = sim->create_part(-3, x + sim->rng.between(-1, 1), y + sim->rng.between(-1, 1), PT_ELEC);
				if (j >= 0)
				{
					parts[j].temp = temp;
				}
			}
			j = sim->create_part(-3, x + sim->rng.between(-1, 1), y + sim->rng.between(-1, 1), PT_PHOT);
			if (j >= 0)
			{
				parts[j].ctype = 0x7C0000;
				parts[j].temp = temp;
				parts[j].tmp = 1;
			}
			j = sim->create_part(-3, x + sim->rng.between(-1, 1), y + sim->rng.between(-1, 1), PT_PLSM);
			if (j >= 0)
			{
				parts[j].temp = temp;
				parts[j].tmp |= 4;
			}
			parts[i].temp = temp + 750 + sim->rng.between(0, 499);
			sim->pv[y / CELL][x / CELL] += 30;
		}
	}
	return 0;
}
