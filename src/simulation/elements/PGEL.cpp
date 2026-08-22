#include "simulation/ElementCommon.h"

#include "simulation/MovingSolid.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_PGEL()
{
	Identifier = "DEFAULT_PT_PGEL";
	Name = "RGEL";
	Colour = 0x00A0FF_rgb;
	MenuVisible = 1;
	MenuSection = SC_LIQUID;
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

	Weight = 100;

	HeatConduct = 29;
	Description = "Repulsion Gel. Stains solids, washes with water. Very bouncy.";

	Properties = TYPE_LIQUID | PROP_LIFE_DEC | PROP_NEUTPENETRATE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;

	DefaultProperties.temp = R_TEMP - 2.0f + 273.15f;
}

static int update(UPDATE_FUNC_ARGS)
{
	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;

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

				// Solid goo
				if (parts[i].tmp)
				{
					parts[i].vx = parts[i].vy = 0.0f;
				}

				// Delete self after while if liquid
				if (!parts[i].tmp && sim->rng.chance(1, 2000))
				{
					sim->kill_part(i);
					return 0;
				}

				// Melt when too hot
				if (parts[i].temp > 7000.0f)
				{
					sim->part_change_type(i, x, y, PT_LAVA);
					return 0;
				}

				// Washing away with water
				if (elements[rt].Properties & PROP_WATER)
				{
					parts[i].tmp = 0;
					sim->part_change_type(i, x, y, parts[i].ctype);
					return 0;
				}

				// Stain with the gel. Basically, don't stain any of the elements above
				if (
					!parts[i].tmp &&
					!(elements[rt].Properties & TYPE_LIQUID) && !(elements[rt].Properties & TYPE_GAS) &&
					!(elements[rt].Properties & PROP_INDESTRUCTIBLE) && !(elements[rt].Properties & PROP_VEHICLE) &&
					rt != PT_BCLN && rt != PT_BRAY && rt != PT_FFLD && rt != PT_FIGH &&
					rt != PT_GEL && rt != PT_MVSD && rt != PT_PCLN && rt != PT_PGEL &&
					rt != PT_PRTI && rt != PT_PRTO && rt != PT_STKM && rt != PT_STKM2
				)
				{
					parts[ID(r)].ctype = rt;
					parts[ID(r)].tmp = 1;
					sim->part_change_type(ID(r), x + rx, y + ry, PT_PGEL);
					sim->kill_part(i);
					return 0;
				}

				// Stain MVSD by changing ctype, but only in liquid form
				if (!parts[i].tmp && rt == PT_MVSD && parts[ID(r)].ctype != PT_PGEL)
				{
					parts[ID(r)].life = parts[ID(r)].ctype;
					parts[ID(r)].ctype = PT_PGEL;
				}

				// If we're not altering another gel particle we can apply what we need to bounce the particle
				if (parts[i].tmp && rt != PT_PGEL)
				{
					if (rt == PT_STKM || rt == PT_STKM2 || rt == PT_FIGH) // Bouncing for stkm
					{
						float gravX, gravY;
						sim->GetGravityField(parts[ID(r)].x, parts[ID(r)].y, 0.3f, 0.3f, gravX, gravY);
						parts[ID(r)].vx -= gravX * std::abs(parts[ID(r)].vx);
						parts[ID(r)].vy -= gravY * std::abs(parts[ID(r)].vy);;
					}
					else if (rt == PT_MVSD && parts[ID(r)].tmp2 > 0 && MovingSolid::solids.count(parts[ID(r)].tmp2)) // Moving solids already bounce, just flag it to bounce higher
					{
						MovingSolid::solids[parts[ID(r)].tmp2].FlagBigBounce();
					}
					else
					{
						parts[ID(r)].vx *= -1.05;
						parts[ID(r)].vy *= -1.05;
					}
				}
			}
		}
	}

	return 0;
}
