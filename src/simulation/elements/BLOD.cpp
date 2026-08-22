#include "simulation/ElementCommon.h"

constexpr int BLOD_CLOT = 180;

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_BLOD()
{
	Identifier = "DEFAULT_PT_BLOD";
	Name = "BLOD";
	Colour = 0xEB1515_rgb;
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

	HeatConduct = 29;
	Description = "Blood. Stains particles, clots when still.";

	Properties = TYPE_LIQUID | PROP_NEUTPENETRATE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
	Graphics = &graphics;

	DefaultProperties.life = 100;
	DefaultProperties.temp = R_TEMP - 2.0f + 273.15f;
	DefaultProperties.tmp = 2;
}

static int update(UPDATE_FUNC_ARGS)
{
	/**
	 * Properties:
	 * life - How much oxygen stored
	 * tmp - How many particles it can still stain
	 * tmp2 - Visocity, slowly increases randomly with contact to air
	 */

	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;

	// Boundng
	if (parts[i].life > 100)
	{
		parts[i].life = 100;
	}

	// Freezing
	if (parts[i].temp < 273.15f)
	{
		sim->part_change_type(i, x, y, PT_ICEI);
		parts[i].ctype = PT_BLOD;
		parts[i].dcolour = elements[PT_BLOD].Colour.Pack() + 0x77000000;
		return 0;
	}

	// Boiling
	if (parts[i].temp > 100.0f + 273.15f)
	{
		if (sim->rng.chance(1, 500))
		{
			sim->part_change_type(i, x, y, PT_BRMT);
		}
		else
		{
			sim->part_change_type(i, x, y, PT_WTRV);
			parts[i].dcolour = elements[PT_BLOD].Colour.Pack() + 0x44000000;
		}
	}

	// Clotted blood is inert
	if (parts[i].tmp2 >= BLOD_CLOT)
	{
		if (parts[i].life > 30)
		{
			parts[i].life--;
		}
		parts[i].vx = parts[i].vy = 0.0f;
	}

	pixel newcolor;
	for (auto rx = -1; rx <= 1; rx++)
	{
		for (auto ry = -1; ry <= 1; ry++)
		{
			if (rx || ry)
			{
				auto r = pmap[y + ry][x + rx];
				auto rt = TYP(r);

				if (rt == PT_SOAP && sim->rng.chance(1, 100))
				{
					sim->kill_part(i);
					continue;
				}

				// Clotted blood has none of these updates
				if (parts[i].tmp2 >= BLOD_CLOT)
				{
					continue;
				}

				if (!r)
				{
					// Random visocity increase if not moving fast and minimal pressure
					if (std::hypot(parts[i].vx, parts[i].vy) < 0.1f && std::abs(sim->pv[y / CELL][x / CELL]) < 1.0f)
					{
						parts[i].tmp2 += sim->rng.between(1, 7);
					}
					continue;
				}

				if (rt == PT_O2 && parts[i].life < 100) // Oxygenate
				{
					parts[i].life += 10;
					sim->kill_part(ID(r));
				}
				else if (rt == PT_BLOD && parts[i].life - 2 > parts[ID(r)].life && parts[ID(r)].tmp2 < BLOD_CLOT) // Spread oxygen to surrounding BLOD particles
				{
					parts[i].life = (parts[ID(r)].life + parts[i].life + 1) / 2;
					parts[ID(r)].life = parts[i].life;
				}
				else if (
					rt != PT_BIZRS && rt != PT_ICEI && rt != PT_SNOW &&
					(elements[rt].Properties & TYPE_SOLID || elements[rt].Properties & TYPE_PART)
				) // Stain solids and powders
				{
					newcolor = elements[PT_BLOD].Colour.Pack() + 0xFF000000;
					if (parts[ID(r)].dcolour != newcolor && parts[i].tmp)
					{
						parts[i].tmp--;
						parts[ID(r)].dcolour = newcolor;
					}
				}
				else if (rt != PT_BIZR && rt != PT_BLOD && rt != PT_SOAP && elements[rt].Properties & TYPE_LIQUID) // Stain liquids. Liquids get stained more, but in a diluted color
				{
					if (parts[i].tmp)
					{
						newcolor = elements[PT_BLOD].Colour.Pack() + 0xAA000000;
						if (parts[ID(r)].dcolour != newcolor && sim->rng.chance(1, 2))
						{
							parts[i].tmp--;
							parts[ID(r)].dcolour = newcolor;
						}
					}
				}
				else if (sim->rng.chance(1, 10) && (rt == PT_VIRS || rt == PT_VRSG || rt == PT_VRSS)) // Chance to kill VIRS
				{
					sim->kill_part(ID(r));
				}
				else if (sim->rng.chance(1, 10) && rt == PT_BCTR) // Chance to kill BCTR
				{
					sim->kill_part(ID(r));
				}
			}
		}
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	if (cpart->tmp2 < BLOD_CLOT)
	{
		*pixel_mode |= PMODE_BLUR;
	}
	*colr *= 0.2f + 0.8f * cpart->life / 100.0f;
	*colg *= 0.2f + 0.8f * cpart->life / 100.0f;
	*colb *= 0.2f + 0.8f * cpart->life / 100.0f;

	return 0;
}
