#include "simulation/ElementCommon.h"
#include "RDMD.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_CRBN()
{
	Identifier = "DEFAULT_PT_CRBN";
	Name = "CRBN";
	Colour = 0x444444_rgb;
	MenuVisible = 1;
	MenuSection = SC_POWDERS;
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

	Flammable = 200;
	Explosive = 0;
	Meltable = 0;
	Hardness = 2;
	PhotonReflectWavelengths = 0x00000000;

	Weight = 90;

	HeatConduct = 10;
	Description = "Carbon dust. Turns into RDMD under heat and pressure, superconducts when cold.";

	Properties = TYPE_PART | PROP_CONDUCTS | PROP_NEUTPASS | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 3600.0f + 273.15f;
	HighTemperatureTransition = PT_CO2;

	Update = &update;
	Graphics = &graphics;

	Create = &create;
}

static int update(UPDATE_FUNC_ARGS)
{
	// Stay still if tmp2
	if (parts[i].tmp2)
	{
		parts[i].vx = parts[i].vy = 0;
	}

	// Solidify into realistic diamond
	if (parts[i].temp > 2500.0f + 273.15f && sim->pv[y / CELL][x / CELL] > 100.0f && sim->rng.chance(1, 20))
	{
		sim->part_change_type(i, x, y, PT_RDMD);
		Element_RDMD_create(sim, i, x, y, PT_RDMD, -1);
		return 0;
	}

	bool seenH2 = false, seenFIRE = false, seenSPRK = false;
	int CRBNCount = 0, xorCheck = 0;

	for (auto rx = -1; rx <= 1; rx++)
	{
		for (auto ry = -1; ry <= 1; ry++)
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
				auto rt = TYP(r);

				if (rt == PT_PSTE) // Solidify
				{
					parts[i].tmp2 = 1;
					sim->kill_part(ID(r));
				}
				else if (rt == PT_WATR) // Purify water
				{
					sim->part_change_type(ID(r), x + rx, y + ry, PT_DSTW);
				}
				else if (rt == PT_SLTW)
				{
					sim->part_change_type(ID(r), x + rx, y + ry, sim->rng.chance(1, 12) ? PT_SALT : PT_DSTW);
				}
				else if (rt == PT_LAVA && parts[ID(r)].ctype == PT_IRON) // Convert molten IRON into METL (steel)
				{
					parts[ID(r)].ctype = PT_METL;
					if (sim->rng.chance(1, 5))
					{
						sim->kill_part(i);
						return 0;
					}
				}
				else if (rt == PT_NEUT) // Slow down neutrons
				{
					parts[ID(r)].vx *= 0.8f;
					parts[ID(r)].vy *= 0.8f;
				}
			}
		}
	}

	// Prevent conduction if conducted from PSCN and NSCN at same time
	for (auto rx = -2; rx <= 2; rx++)
	{
		for (auto ry = -2; ry <= 2; ry++)
		{
			if (rx || ry)
			{
				auto r = pmap[y + ry][x + rx];
				if (!r)
				{
					continue;
				}
				auto rt = TYP(r);

				if (rt == PT_FIRE || rt == PT_PLSM)
				{
					seenFIRE = true;
				}
				else if (rt == PT_H2)
				{
					seenH2 = true;
				}
				else if (rt == PT_SPRK)
				{
					seenSPRK = true;
					if (parts[ID(r)].ctype == PT_PSCN)
					{
						xorCheck |= 1;
					}
					else if (parts[ID(r)].ctype == PT_NSCN)
					{
						xorCheck |= 2;
					}
				}
				else if (rt == PT_CRBN)
				{
					CRBNCount++;
				}
			}
		}
	}

	// Prevent conduction if both PSCN and NSCN
	if (xorCheck == 3)
	{
		parts[i].life = 1;
	}

	if (seenH2 && (seenFIRE || (parts[i].temp > 10.0f + 273.15f && sim->rng.chance(1, 200)))) // H2 + FIRE + CRBN = GAS
	{
		parts[i].temp += 5.0f;
		sim->part_change_type(i, x, y, PT_GAS);
	}
	else if (seenFIRE && sim->rng.chance(1, 20)) // FIRE + CRBN = CO2
	{
		sim->part_change_type(i, x, y, PT_CO2);
	}
	else if (parts[i].temp < 100.0f && seenSPRK && CRBNCount <= 2 && xorCheck != 3) // Thin wires superconduct
	{
		sim->FloodINST(x, y, PT_CRBN);
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	int z = (cpart->tmp - 5) * 5; // Speckles!

	*colr += z;
	*colg += z;
	*colb += z;

	return 0;
}

static void create(ELEMENT_CREATE_FUNC_ARGS)
{
	sim->parts[i].tmp = sim->rng.between(0, 6);
}
