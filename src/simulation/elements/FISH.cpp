#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_FISH()
{
	Identifier = "DEFAULT_PT_FISH";
	Name = "FISH";
	Colour = 0x8DAFBA_rgb;
	MenuVisible = 1;
	MenuSection = SC_ORGANIC;
	Enabled = 1;

	Advection = 0.1f;
	AirDrag = 0.003f * CFDS;
	AirLoss = 0.94f;
	Loss = 1.00f;
	Collision = -0.1f;
	Gravity = 0.3f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 30;
	Explosive = 1;
	Meltable = 0;
	Hardness = 2;

	Weight = 30;

	HeatConduct = 150;
	Description = "Freshwater fish, swims in water and eats ANT and SEED.";

	Properties = TYPE_PART | PROP_EDIBLE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = 6.0f;
	HighPressureTransition = PT_DUST;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 90.0f + 273.15f;
	HighTemperatureTransition = PT_DUST;

	FoodValue = 3;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS)
{
	/**
	 * Literally just move around randomly
	 * Flock like behavior is difficult with liquid swapping
	 *
	 * tmp3/4 = velocity vector
	 */

	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;

	// Update new velocity if still or no new velocity
	if (!(parts[i].tmp3 || parts[i].tmp4) || sim->rng.chance(1, 3))
	{
		if (sim->rng.chance(1, 2))
		{
			parts[i].tmp3 = sim->rng.chance(1, 2) ? -1 : 1;
			parts[i].tmp4 = 0;
		}
		else
		{
			parts[i].tmp4 = sim->rng.chance(1, 2) ? -1 : 1;
			parts[i].tmp3 = 0;
		}
	}

	bool touching_water = false;

	for (auto rx = -2; rx < 3; rx++)
	{
		for (auto ry = -2; ry < 3; ry++)
		{
			if (rx || ry)
			{
				auto r = pmap[y + ry][x + rx];

				// Redirect velocity away from air
				if (!r)
				{
					parts[i].tmp3 = -isign(rx);
					parts[i].tmp4 = -isign(ry);
					continue;
				}

				auto rt = TYP(r);

				// Redirect velocity towards SEED and ANT
				if (rt == PT_SEED || rt == PT_ANT)
				{
					parts[i].tmp3 = isign(rx);
					parts[i].tmp4 = isign(ry);
				}

				// Water touch check
				if (abs(rx) < 2 && abs(ry) < 2)
				{
					if (elements[rt].Properties & PROP_WATER)
					{
						touching_water = true;
					}
					else if ((rt == PT_ANT || rt == PT_SEED) && sim->rng.chance(1, 40)) // Eat SEED and ANT
					{
						sim->kill_part(ID(r));
					}
				}
			}
		}
	}

	// Chance to die if not touching water
	if (!touching_water && sim->rng.chance(1, 600))
	{
		sim->part_change_type(i, x, y, PT_DUST);
		return 0;
	}

	// Try to swap with liquid at new velocity
	int nx = x + parts[i].tmp3;
	int ny = y + parts[i].tmp4;
	if ((nx != x || ny != y) && nx >= 0 && nx < XRES && ny >= 0 && ny < YRES)
	{
		auto r = pmap[ny][nx];

		// Cannot move there, pick new velocity next frame by resetting tmp3/4 to 0
		if (!r)
		{
			parts[i].tmp3 = parts[i].tmp4 = 0;
			return 0;
		}

		auto rt = TYP(r);

		if (elements[rt].Properties & TYPE_LIQUID)
		{
			auto nxf = parts[ID(r)].x;
			auto nyf = parts[ID(r)].y;
			sim->move(ID(r), nx, ny, parts[i].x, parts[i].y);
			if (!sim->move(i, x, y, nxf, nyf))
			{
				return 1;
			}
			return 0;
		}
	}

	return 0;
}
