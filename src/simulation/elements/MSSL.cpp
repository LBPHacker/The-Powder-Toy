#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_MSSL()
{
	Identifier = "DEFAULT_PT_MSSL";
	Name = "MSSL";
	Colour = 0x877A6D_rgb;
	MenuVisible = 0;
	MenuSection = SC_SPECIAL;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.00f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.5f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 5;

	Weight = 100;

	HeatConduct = 35;
	Description = "Guided missile. Hidden element.";

	Properties = TYPE_PART | PROP_NEUTPASS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 2614.0f + 273.15f;
	HighTemperatureTransition = PT_BMTL;

	Update = &update;

	DefaultProperties.life = 10;
}

static int update(UPDATE_FUNC_ARGS)
{
	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;

	auto detonate = [&sim, x, y]() {
		sim->create_part(-1, x + 1, y + 1, PT_BMTL);
		sim->create_part(-1, x + 1, y - 1, PT_BMTL);
		sim->create_part(-1, x - 1, y + 1, PT_BMTL);
		sim->create_part(-1, x - 1, y - 1, PT_BMTL);
		sim->create_part(-1, x,     y + 1, PT_BMTL);
		sim->create_part(-1, x,     y - 1, PT_BMTL);
		sim->create_part(-1, x - 1, y,     PT_BMTL);
		sim->create_part(-1, x + 1, y,     PT_BMTL);
	};

	// Not primed yet
	if (parts[i].life > 0)
	{
		parts[i].life--;
		return 0;
	}

	// Align self towards target
	float angle = std::atan2(parts[i].tmp4 - y, parts[i].tmp3 - x);
	parts[i].vx += 2.0f * std::cos(angle);
	parts[i].vy += 2.0f * std::sin(angle);

	// Create exhaust
	int px = parts[i].x + 0.5f - parts[i].vx;
	int py = parts[i].y + 0.5f - parts[i].vy;
	if (px >= 0 && px < XRES && py >= 0 && py < YRES)
	{
		sim->create_part(-1, px, py, PT_SMKE);
	}

	// Should it explode now?
	int dis = (parts[i].tmp3 - x) * (parts[i].tmp3 - x) + (parts[i].tmp4 - y) * (parts[i].tmp4 - y);
	if (dis <= 4)
	{
		sim->part_change_type(i, x, y, PT_BOMB);
		detonate();
		return 0;
	}

	int tx = parts[i].x + 0.5f + parts[i].vx;
	int ty = parts[i].y + 0.5f + parts[i].vy;
	if (tx >= 0 && tx < XRES && ty >= 0 && ty < YRES)
	{
		// If target location is a liquid or GAS swap places
		auto r = pmap[ty][tx];
		if (r && (elements[TYP(r)].Properties & TYPE_LIQUID || elements[TYP(r)].Properties & TYPE_GAS))
		{
			auto nxf = parts[ID(r)].x;
			auto nyf = parts[ID(r)].y;
			sim->move(ID(r), tx, ty, parts[i].x, parts[i].y);
			if (!sim->move(i, x, y, nxf, nyf))
			{
				return 1;
			}
			return 0;
		}
	}

	// Check for nearby solids and powders
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

				// Explode on contact
				if (rt != PT_MSSL && (elements[rt].Properties & TYPE_SOLID || elements[rt].Properties & TYPE_PART || rt == PT_FFLD))
				{
					sim->part_change_type(i, x, y, PT_BOMB);
					detonate();
					return 0;
				}
			}
		}
	}

	return 0;
}
