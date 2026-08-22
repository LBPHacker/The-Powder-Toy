#include "simulation/ElementCommon.h"

constexpr int MAX_WEB_DISTANCE = 15;

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_WEB()
{
	Identifier = "DEFAULT_PT_WEB";
	Name = "WEB";
	Colour = 0x333333_rgb;
	MenuVisible = 1;
	MenuSection = SC_ORGANIC;
	Enabled = 1;

	Advection = 0.05f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.97f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 0;

	Flammable = 20;
	Explosive = 0;
	Meltable = 0;
	Hardness = 12;

	Weight = 100;

	HeatConduct = 75;
	Description = "Spider web. Sticky, captures light powders and prey.";

	Properties = TYPE_SOLID | PROP_NEUTPENETRATE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 500.0f + 273.15f;
	HighTemperatureTransition = PT_FIRE;

	Update = &update;
	Graphics = &graphics;

	Create = &create;

	DefaultProperties.tmp2 = -1;
}

static int update(UPDATE_FUNC_ARGS)
{
	/**
	 * Properties:
	 * - ctype: fake trapped particle
	 * - tmp:  random color (1 - 4, if 100 glistens)
	 * - tmp2: ID of next connection
	 */

	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;

	bool touching = false;
	int particlesSupporting = 0;

	// Break connection if too far or no longer exists
	if (parts[i].tmp2 >= 0 && (!parts[parts[i].tmp2].type || std::hypot(parts[parts[i].tmp2].x - x, parts[parts[i].tmp2].y - y) > MAX_WEB_DISTANCE))
	{
		parts[i].tmp2 = -1;
	}

	// Ctype is fake web, doesn't actually stick
	if (parts[i].ctype)
	{
		return 0;
	}

	for (auto rx = -1; rx <= 1; rx++)
	{
		for (auto ry = -1; ry <= 1; ry++)
		{
			auto r = pmap[y + ry][x + rx];
			if (!r)
			{
				continue;
			}
			auto rt = TYP(r);

			// Same space as another web or element
			if ((!(rx || ry) && rt == PT_WEB && ID(r) != i) || sim->pmap_count[y][x] > 3)
			{
				sim->kill_part(i);
				return 1;
			}

			// Connect to first web or solid it sees
			// If connecting to web must be a connected web
			if (parts[i].tmp2 < 0 && ((rt != PT_WEB && elements[rt].Properties & TYPE_SOLID) || (rt == PT_WEB && parts[ID(r)].tmp2 >= 0)))
			{
				parts[i].tmp2 = ID(r);
			}

			// Set touching property for decay
			if ((rx || ry) && elements[rt].Properties & TYPE_SOLID)
			{
				touching = true;
			}

			bool isAlive = rt == PT_BIRD || rt == PT_BEE || rt == PT_ANT || rt == PT_SPDR;

			// Stop particles if possible
			if (particlesSupporting < 2 && !isAlive && (elements[rt].Properties & TYPE_PART || elements[rt].Properties & TYPE_LIQUID) && elements[rt].Weight < 50)
			{
				particlesSupporting++;
				parts[ID(r)].vx = parts[ID(r)].vy = 0.0f;
			}

			// Capture BIRD, ANT and BEE as WEB randomly
			if (sim->rng.chance(1, 100) && isAlive && rt != PT_SPDR)
			{
				sim->part_change_type(ID(r), x + rx, y + ry, PT_WEB);
				parts[ID(r)].ctype = rt;
				parts[ID(r)].vx = parts[ID(r)].vy = 0.0f;
			}
		}
	}

	if (!touching && sim->rng.chance(1, 10))
	{
		if (parts[i].ctype)
		{
			sim->part_change_type(i, x, y, parts[i].ctype);
		}
		else
		{
			sim->kill_part(i);
		}
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;

	// Mimic ctype color
	if (cpart->ctype > 0 && cpart->ctype < PT_NUM && elements[cpart->ctype].Enabled)
	{
		auto c = elements[cpart->ctype].Colour;
		*colr = c.Red;
		*colg = c.Green;
		*colb = c.Blue;
		return 0;
	}

	*cola = 10;
	if (cpart->tmp == 100) // The one glistening piece of web you see
	{
		*colr = *colg = *colb = 155;
		*pixel_mode |= PMODE_SPARK;
	}
	else
	{
		*colr *= cpart->tmp / 3.0f;
		*colg *= cpart->tmp / 3.0f;
		*colb *= cpart->tmp / 3.0f;
	}

	// Connect web strands if far apart
	int ox = gfctx.sim->parts[cpart->tmp2].x;
	int oy = gfctx.sim->parts[cpart->tmp2].y;
	if (cpart->tmp2 >= 0 && (std::abs((float)cpart->x - ox) > 2.0f || std::abs((float)cpart->y - oy) > 2.0f))
	{
		gfctx.renderer->DrawLine({ (int)cpart->x, (int)cpart->y }, { ox, oy }, RGB(*colr, *colg, *colb));
	}
	return 0;
}

static void create(ELEMENT_CREATE_FUNC_ARGS)
{
	// Randomize color
	sim->parts[i].tmp = sim->rng.between(1, 4);
	if (sim->rng.chance(1, 300))
	{
		sim->parts[i].tmp = 100;
	}
}
