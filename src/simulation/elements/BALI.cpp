#include "simulation/ElementCommon.h"
#include "BALI.h"

#include "ultimata/ElementUtils.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);

// SPOILERS
// Do not read this element code unless you want to be spoiled for Liu Cixin's Ball Lightning (novel)

// Ball lightning
// Invisible until it charges up with energy and glows a brilliant white (Depends on how much it was charged)
// It's energy output is electricity like LIGH/SPRK
// Emits a circle with same color
// Goes against air flow and after random amount of time, will explode
// Explosion will destroy only it's ctyle nearby replacing it with something that glows then turns into ash

// Marker life value, r, g, b
// For example, if life = 400, the color would be eased between the color at life = 380 and life = 450
constexpr int BALI_WAVELENGTHS[9][4] = {
	{ 0,   0,   0,   0   },
	{ 380, 128, 0,   128 },
	{ 450, 0,   0,   255 },
	{ 485, 0,   128, 128 },
	{ 500, 0,   255, 0   },
	{ 565, 128, 128, 0   },
	{ 590, 255, 165, 0   },
	{ 625, 255, 0,   0   },
	{ 825, 255, 255, 255 }
};

// Default ctypes to spawn with to destroy
constexpr std::array<int, 13> BALI_DEFAULT_CTYPES = { PT_PLNT, PT_IRON, PT_METL, PT_IRON, PT_TTAN, PT_GOLD, PT_GLAS, PT_QRTZ, PT_BRCK, PT_CRMC, PT_WOOD, PT_GOO, PT_SAND };

void Element_BALI_findBoundary(int life, int& colr, int& colg, int& colb, int &cola)
{
	for (auto i = 0; i < 8; i++)
	{
		if (life > BALI_WAVELENGTHS[i][0] && life > BALI_WAVELENGTHS[i + 1][0])
		{
			// Stop at first life we're between 2 boundaries
			continue;
		}

		// Linear easing, we take the range between the 2 colors
		// and do a proportional averaging, ie, if:
		// 		life = 100 and r1 = 255
		// 		life = 200 and r2 = 100
		// and life is currently 125 (25% from 255 to 0)
		// we compute new life value by taking (life - min) / range * (rdiff) + r2
		// (125 - 100) / (200 - 100) * (100 - 255) + 255
		// = 0.25 * -155 + 255 = 216.75

		float lifediff = BALI_WAVELENGTHS[i + 1][0] - BALI_WAVELENGTHS[i][0];
		int rdiff = BALI_WAVELENGTHS[i + 1][1] - BALI_WAVELENGTHS[i][1];
		int gdiff = BALI_WAVELENGTHS[i + 1][2] - BALI_WAVELENGTHS[i][2];
		int bdiff = BALI_WAVELENGTHS[i + 1][3] - BALI_WAVELENGTHS[i][3];

		colr = (life - BALI_WAVELENGTHS[i][0]) / lifediff * rdiff + BALI_WAVELENGTHS[i][1];
		colg = (life - BALI_WAVELENGTHS[i][0]) / lifediff * gdiff + BALI_WAVELENGTHS[i][2];
		colb = (life - BALI_WAVELENGTHS[i][0]) / lifediff * bdiff + BALI_WAVELENGTHS[i][3];
		cola = i == 0 ? (life - BALI_WAVELENGTHS[i][0]) / lifediff * 255 : 255;
		return;
	}

	// It's white now
	colr = colg = colb = 255;
}

void Element::Element_BALI()
{
	Identifier = "DEFAULT_PT_BALI";
	Name = "BALI";
	Colour = 0xDFEFFF_rgb;
	MenuVisible = 1;
	MenuSection = SC_NUCLEAR;
	Enabled = 1;

	Advection = -0.7f;
	AirDrag = -0.01f * CFDS;
	AirLoss = 0.96f;
	Loss = 0.50f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.50f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;

	Weight = 1;

	HeatConduct = 251;
	Description = "Ball Lightning. Actually a macroelectron, releases energy onto its ctype.";

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
	Graphics = &graphics;

	Create = &create;
}

static int update(UPDATE_FUNC_ARGS)
{
	/**
	 * Properties:
	 * life = energy level
	 * ctype = target element to act upon
	 * tmp = Time before explosion
	 * tmp2 = If not 0, will simply stay still and glow then disappear
	 */

	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;

	// Stay still if tmp2
	if (parts[i].tmp2)
	{
		parts[i].vx = parts[i].vy = 0.0f;
	}

	parts[i].tmp--;
	if (parts[i].tmp <= 0 || parts[i].life <= 0)
	{
		// Explode if enough power
		if (parts[i].life > 100)
		{
			// Create smoke and pressure
			sim->pv[y / CELL][x / CELL] += 60.0f;
			sim->create_part(-1, x - 1, y, PT_SMKE);
			sim->create_part(-1, x + 1, y, PT_SMKE);
			sim->create_part(-1, x, y - 1, PT_SMKE);
			sim->create_part(-1, x, y + 1, PT_SMKE);

			// Radius is equal to round(life / 20)
			// Deletion radius caps out at 60 px
			int radius = std::min((int)(parts[i].life / 20.0f + 0.5f), 60);

			for (auto rx = -radius; rx <= radius; rx++)
			{
				for (auto ry = -radius; ry <= radius; ry++)
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

					if (TYP(r) == parts[i].ctype && std::hypot(rx, ry) <= radius)
					{
						sim->part_change_type(ID(r), x + rx, y + ry, PT_BALI);
						parts[ID(r)].life = 15;
						parts[ID(r)].ctype = TYP(r);
						parts[ID(r)].tmp = 15;
						parts[ID(r)].tmp2 = 1;
					}
				}
			}
		}

		if (parts[i].tmp2 && sim->rng.chance(1, 50)) // Turn to ash
		{
			sim->part_change_type(i, x, y, PT_BCOL);
		}
		else // Actual BALI disappears
		{
			sim->kill_part(i);
		}

		return 0;
	}

	if (!parts[i].tmp2 && parts[i].life >= 0)
	{
		for (auto rx = -3; rx <= 3; rx++)
		{
			for (auto ry = -3; ry <= 3; ry++)
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

					// Electric elements increase BALI's energy level
					// SPRK was removed since it can SPRK elements and supercharge itself
					if (TYP(r) == PT_ELEC)
					{
						parts[i].life++;
					}
					else if (TYP(r) == PT_LIGH || TYP(r) == PT_THDR)
					{
						parts[i].life += sim->rng.between(380, 600);
					}

					// BALI absorbs other BALI only if this has more energy
					if (TYP(r) == PT_BALI && parts[i].life >= parts[ID(r)].life)
					{
						parts[i].life += parts[ID(r)].life;
						sim->kill_part(ID(r));
						return 0;
					}

					// Randomly spark elements if there are
					if (sim->rng.chance(1, 20))
					{
						if (elements[TYP(r)].Properties & PROP_CONDUCTS)
						{
							sim->part_change_type(ID(r), x + rx, y + ry, PT_SPRK);
							parts[ID(r)].life = 4;
							parts[ID(r)].ctype = TYP(r);
							parts[i].life -= 10;
						}
					}
				}
			}
		}
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	// Stand still and glow, we can just glow this 1 px
	if (cpart->tmp2)
	{
		*pixel_mode |= FIRE_ADD;
		*colr = *colg = *colb = 255;
		*firer = *fireg = *fireb = 255;
		*firea = 100;
		return 0;
	}

	*colr = *colg = *colb = 0;
	*cola = 255; // Black

	Element_BALI_findBoundary(cpart->life, *colr, *colg, *colb, *cola);

	for (auto rx = -6; rx <= 6; rx++)
	{
		for (auto ry = -6; ry <= 6; ry++)
		{
			float radius = std::hypot(rx, ry) * (1.0f + 1.5f * gfctx.rng.uniform01());
			if (radius <= 4 + std::sin(gfctx.sim->currentTick / 20.0f)) // Radius of inner circle, pulsates between 3 and 5. White glow inside
			{
				DrawGlowyPixel(gfctx.renderer, cpart->x + rx, cpart->y + ry, 255, 255, 255, *cola);
			}
			else if (radius <= 6) // Radius 6, outer glow (colored)
			{
				DrawGlowyPixel(gfctx.renderer, cpart->x + rx, cpart->y + ry, *colr, *colg, *colb, *cola);
			}
		}
	}

	// Actual middle particle is invisible, make it white to blend in if its white
	if (*cola != 0)
	{
		*colr = *colg = *colb = 255;
	}

	return 0;
}

static void create(ELEMENT_CREATE_FUNC_ARGS)
{
	sim->parts[i].life = sim->rng.between(380, 625);
	sim->parts[i].tmp = sim->rng.between(600, 4999);

	// Random ctype to destroy
	sim->parts[i].ctype = BALI_DEFAULT_CTYPES[sim->rng.between(0, BALI_DEFAULT_CTYPES.size() - 1)];
}
