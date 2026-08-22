#include "simulation/ElementCommon.h"
#include "RDMD.h"

#include "ultimata/ElementUtils.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_RDMD()
{
	Identifier = "DEFAULT_PT_RDMD";
	Name = "RDMD";
	Colour = 0xD9FBFF_rgb;
	MenuVisible = 1;
	MenuSection = SC_SOLIDS;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 1;
	Hardness = 0;

	Weight = 100;

	HeatConduct = 150;
	Description = "Realistic Diamond. Tough but destroyable. Slightly flammable.";

	Properties = TYPE_SOLID | PROP_PHOTPASS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 4027.0f + 273.15f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
	Graphics = &graphics;

	Create = &Element_RDMD_create;
}

static int update(UPDATE_FUNC_ARGS)
{
	/**
	 * Props: tmp3: diamond graphics multiplier
	 * Life: on fire if > 0, dies if life == 1
	 */

	if (parts[i].life > 1)
	{
		parts[i].life--;
	}

	// Die
	if (parts[i].life == 1)
	{
		sim->part_change_type(i, x, y, PT_CO2);
		return 0;
	}

	for (auto rx = -1; rx <= 1; rx++)
	{
		for (auto ry = -1; ry <= 1; ry++)
		{
			if (rx || ry)
			{
				auto r = pmap[y + ry][x + rx];
				if (r)
				{
					auto rt = TYP(r);

					if ((rt == PT_FIRE || rt == PT_LAVA || rt == PT_PLSM) && !parts[i].life && sim->rng.chance(1, 30))
					{
						parts[i].life = sim->rng.between(50, 850);
					}
				}
				else
				{
					// Produce fire
					if (parts[i].life && sim->rng.chance(1, 10))
					{
						auto j = sim->create_part(-1, x + rx, y + ry, PT_FIRE);
						if (j >= 0)
						{
							parts[j].life = sim->rng.between(0, 50);
							parts[j].temp = parts[i].temp + 100.0f;
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
	*colr *= 0.8f + 0.2f * ToFloat(cpart->tmp3);
	*colg *= 0.8f + 0.2f * ToFloat(cpart->tmp3);
	*colb *= 0.9f + 0.1f * ToFloat(cpart->tmp3);

	return 0;
}

void Element_RDMD_create(ELEMENT_CREATE_FUNC_ARGS)
{
	constexpr int CELL_SIZE = 30;

	int x1 = sim->parts[i].x;
	int y1 = sim->parts[i].y;
	int t1 = std::hypot(x1 / CELL_SIZE, y1 / CELL_SIZE); // "Seed the x y"

	// Constants were randomly chosen, rotate the above point
	float theta = t1 % 13 / 13.0f * 2.0f * std::numbers::pi_v<float>;
	int x2 = std::cos(theta) * (x1 - XRES / 2) - std::sin(theta) * (y1 - YRES / 2) + XRES / 2;
	int y2 = std::sin(theta) * (x1 - XRES / 2) + std::cos(theta) * (y1 - YRES / 2) + YRES / 2;

	// Recalculate after rotation
	t1 = std::hypot(x2 / CELL_SIZE, y2 / CELL_SIZE);
	t1 += y2 / CELL_SIZE; // Avoid repeating diagonals

	if (x2 % CELL_SIZE < y2 % CELL_SIZE) // Avoid parallelograms - this cuts them into 2 triangles of different color
	{
		t1++;
	}

	// Set seed value
	sim->parts[i].tmp3 = FromFloat((t1 * 55) % 255 / 255.0f);
}
