#include "simulation/ElementCommon.h"

#include "graphics/Renderer.h"
#include "simulation/Vehicle.h"

constexpr int BEE_SEARCH_RANGE = 3;
constexpr float BEE_MAX_BUILD_SPEED = 1.0f;
constexpr float BEE_MAX_SPEED = 2.0f;
constexpr int BEE_MAX_RADIUS_TO_MAKE_WAX = 20;
constexpr int BEE_EDGE_RANGE = 20;
constexpr int BEE_SWARM_RANGE = 90;
constexpr float BEE_PREFERED_TEMP = 30.0f + 273.15f;

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_BEE()
{
	Identifier = "DEFAULT_PT_BEE";
	Name = "BEE";
	Colour = 0xFFC414_rgb;
	MenuVisible = 1;
	MenuSection = SC_ORGANIC;
	Enabled = 1;

	Advection = 0.7f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.96f;
	Loss = 1.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.01f;
	HotAir = 0.001f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 20;

	HeatConduct = 70;
	Description = "Bees. Pollinates PLNT, makes honey and wax hives where it spawns.";

	Properties = TYPE_GAS | PROP_DEADLY;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = 10.0f;
	HighPressureTransition = PT_DUST;
	LowTemperature = -5.0f + 273.15f;
	LowTemperatureTransition = PT_SNOW;
	HighTemperature = 70.0f + 273.15f;
	HighTemperatureTransition = PT_DUST;

	Update = &update;
	Graphics = &graphics;

	Create = &create;
}

static void create(ELEMENT_CREATE_FUNC_ARGS)
{
	sim->parts[i].tmp2 = sim->rng.chance(1, 2);
	sim->parts[i].dcolour = sim->rng.between(0, 7);

	// Queen bee
	if (sim->rng.chance(1, 500))
	{
		sim->parts[i].tmp2 = 2;
	}

	sim->parts[i].tmp3 = (int)(sim->parts[i].x + 0.5f);
	sim->parts[i].tmp4 = (int)(sim->parts[i].y + 0.5f);
}

static int update(UPDATE_FUNC_ARGS)
{
	/**
	 * Properties:
	 * tmp    - color variation (0 - 6 inclusive)
	 * tmp2   - working stage, bees alternate between finding food and
	 *          tending for the hive. 0 = work, 1 = find
	 * life   - pollen level, 0 to 100, stops pollinating when at 100
	 * tmp3/4 - Spawn coordinates
	 * flags  - aggressive? SMKE sets this to 100, decrements each frame
	 */

	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;

	bool freeze = false; // Dont move (when pollinating)
	bool alreadyPollinated = false; // Already pollinated a plant this frame

	if (parts[i].flags)
	{
		parts[i].flags--;
	}

	// Queen bee behavior differs: spawn a random worker every 500 frames
	if (parts[i].tmp2 == 2 && sim->frameCount % 500 == 0)
	{
		parts[i].vx = parts[i].vy = 0.0f;

		for (auto rx = -1; rx <= 1; rx++)
		{
			for (auto ry = -1; ry <= 1; ry++)
			{
				if (rx || ry)
				{
					auto r = pmap[y + ry][x + rx];
					if (!r)
					{
						auto j = sim->create_part(-1, x + rx, y + ry, PT_BEE);
						parts[j].tmp2 = sim->rng.chance(1, 2); // New bee cannot be queen
						parts[j].tmp3 = parts[i].tmp3;
						parts[j].tmp4 = parts[i].tmp4;
						return 0;
					}
				}
			}
		}
		return 0;
	}

	// Normal worker behavior
	float tendx = 0.0f, tendy = 0.0f;
	int objectCount = 0;

	bool nearHive = std::hypot(x - parts[i].tmp3, y - parts[i].tmp4) < BEE_MAX_RADIUS_TO_MAKE_WAX;

	// For constructing wax honeycomb, it won't construct if another
	// solid is near (bee space)
	int waxrx = 0, waxry = 0;
	bool shouldMakeWax = true;

	// Avoid edges
	if (x < BEE_EDGE_RANGE)
	{
		tendx += 10.0f;
	}
	if (y < BEE_EDGE_RANGE)
	{
		tendy += 10.0f;
	}
	if (x > XRES - BEE_EDGE_RANGE)
	{
		tendx -= 10.0f;
	}
	if (y > YRES - BEE_EDGE_RANGE)
	{
		tendy -= 10.0f;
	}
	if (tendx || tendy)
	{
		objectCount = 1;
	}

	// Follow dance if contained
	if (parts[i].tmp && parts[i].tmp % 2 && parts[i].tmp2 == 1 && !parts[i].life)
	{
		float angle = (parts[i].tmp % 1000) / 180.0f * std::numbers::pi_v<float>;
		tendx -= 2.0f * std::cos(angle);
		tendy -= 2.0f * std::sin(angle);
	}

	// Randomly "fail" and decide to return to hive
	if (sim->rng.chance(1, 5000))
	{
		parts[i].tmp = -1;
	}

	for (auto rx = -BEE_SEARCH_RANGE; rx <= BEE_SEARCH_RANGE; rx++)
	{
		for (auto ry = -BEE_SEARCH_RANGE; ry <= BEE_SEARCH_RANGE; ry++)
		{
			if (rx || rx)
			{
				auto r = pmap[y + ry][x + rx];
				if (!r)
				{
					// Builder, try to expand the hive here
					// Hives follow a hexagon pattern:
					//  xx  xx
					// x  xx  x
					//  xx  xx
					if (
						!(waxrx || waxry) &&
						std::abs(rx) < 2 && std::abs(ry) < 2 &&     // Right next to spot and not already building WAX
						((x + rx) / 2) % 2 == (y + ry) % 2 &&       // Hexagon shape
						!parts[i].tmp2 && sim->rng.chance(1, 50) && // Is building
						nearHive                                    // Dont make too far from spawn
					)
					{
						waxrx = rx;
						waxry = ry;
					}
					continue;
				}
				auto rt = TYP(r);

				// Shouldn't make WAX if near non-HONY non-WAX solid or powder
				if (
					(rt != PT_BEE && rt != PT_HONY && rt != PT_WAX) &&
					(elements[rt].Properties & TYPE_SOLID || elements[rt].Properties & TYPE_PART)
				)
				{
					shouldMakeWax = false;
				}

				// Instantly get zapped by electricity
				if (rt == PT_LIGH || rt == PT_SPRK)
				{
					sim->part_change_type(i, x, y, PT_DUST);
					return 0;
				}

				// Passive if smoked
				if (rt == PT_SMKE)
				{
					parts[i].flags = 100;
				}

				if (parts[i].tmp2 == 1) // Foraging
				{
					if (rt == PT_PLNT || rt == PT_SUGR || rt == PT_SWTR || rt == PT_WATR) // Head towards PLNT, SUGR, WATR and SWTR
					{
						if (parts[i].life < 100)
						{
							tendx += rx, tendy += ry;
						}
						else // Go away if full to leave room for other bees
						{
							tendx -= rx;
							tendy -= ry;
						}
						objectCount++;

						// Pollinate PLNT, fuel up on SUGR or water
						// Can only pollinate 1px of PLNT at a time
						if (std::abs(rx) < 2 && std::abs(ry) < 2 && parts[i].life < 100 && !alreadyPollinated)
						{
							freeze = alreadyPollinated = true;
							parts[i].life++;

							// Encode dance data
							if (!parts[i].tmp || parts[i].tmp % 2)
							{
								int dis = std::hypot(x - parts[i].tmp3, y - parts[i].tmp4);
								int angle = std::atan2(-y + parts[i].tmp4, x - parts[i].tmp3) * 180.0f / std::numbers::pi_v<float>;
								while (angle < 0)
								{
									angle += 360;
								}
								while (angle > 360)
								{
									angle -= 360;
								}
								parts[i].tmp = angle + 1000 * dis;

								// Make sure tmp % 2 = 0
								if (parts[i].tmp % 2)
								{
									parts[i].tmp++;
								}
							}

							if (rt == PT_PLNT && sim->rng.chance(1, 500)) // Pollinate PLNT (makes it grow)
							{
								sim->part_change_type(ID(r), x + rx, y + ry, PT_VINE);
							}
							else if ((rt == PT_SUGR || rt == PT_SWTR || rt == PT_WATR) && sim->rng.chance(2, 100)) // Consume SUGR or SWTR
							{
								sim->kill_part(ID(r));
							}
						}
					}
					else if (rt == PT_BEE || rt == PT_HONY) // Dont avoid other BEEs, WAX and HONY
					{
						if (parts[ID(r)].temp > BEE_PREFERED_TEMP && sim->rng.chance(1, 100))
						{
							parts[ID(r)].temp--;
						}
						else if (parts[ID(r)].temp > BEE_PREFERED_TEMP && sim->rng.chance(1, 100))
						{
							parts[ID(r)].temp++;
						}

						// Listen to other bees' dances
						if (
							rt == PT_BEE && parts[ID(r)].tmp2 == 1 && // Other bee is dancing
							parts[ID(r)].tmp && parts[ID(r)].tmp % 2 == 0 &&
							parts[i].tmp <= 0 && parts[i].tmp2 == 1 && nearHive && !parts[i].life // Self is ready to find again
						)
						{
							parts[i].tmp = parts[ID(r)].tmp + 1; // Make sure % 2 == 1
						}
					}
					else if (rt == PT_WAX)
					{
						// Regulate temperature
						if (parts[ID(r)].temp > BEE_PREFERED_TEMP && sim->rng.chance(1, 100))
						{
							parts[ID(r)].temp--;
						}
						else if (parts[ID(r)].temp > BEE_PREFERED_TEMP && sim->rng.chance(1, 100))
						{
							parts[ID(r)].temp++;
						}

						// We found some WAX, if near spawn lets deposit honey on it
						if (nearHive && parts[i].life >= 100)
						{
							for (auto rx2 = -1; rx2 <= 1; rx2++)
							{
								for (auto ry2 = -1; ry2 <= 1; ry2++)
								{
									if (rx || ry)
									{
										r = pmap[y + ry + ry2][x + rx + rx2];

										// Deposit honey
										if (!r)
										{
											parts[i].life = 0;
											sim->create_part(-1, x + rx + rx2, y + ry + ry2, PT_HONY);
											return 0;
										}
									}
								}
							}
							return 0;
						}
					}
					else if (rt != PT_WEB && (!(elements[rt].Properties & TYPE_GAS) || parts[ID(r)].temp > 50.0f + 273.15f)) // Avoid non-gases or hot objects
					{
						tendx -= rx;
						tendy -= ry;
						objectCount++;
					}
				}
			}
		}
	}

	// Honeycomb construction
	if (shouldMakeWax && (waxrx || waxry))
	{
		sim->create_part(-1, x + waxrx, y + waxry, PT_WAX);
	}

	// Swarm the player
	if (!parts[i].flags)
	{
		int tarx = -1, tary = -1;
		Vehicle::GetTarget(sim, parts, tarx, tary);

		if (tarx >= 0 && tary >= 0 && std::hypot(x - tarx, y - tary) < BEE_SWARM_RANGE)
		{
			parts[i].vx -= (x - tarx) / 800.0f;
			parts[i].vy -= (y - tary) / 800.0f;
		}
	}

	if (nearHive && sim->rng.chance(1, 100)) // Reset "give up" if near spawn
	{
		parts[i].tmp = 0;
	}

	if (freeze)
	{
		parts[i].vx = parts[i].vy = 0.0f;
	}
	else
	{
		// Tend towards / away from objects
		if (objectCount)
		{
			parts[i].vx += tendx / objectCount;
			parts[i].vy += tendy / objectCount;
		}

		// Dancing overrides leaving the hive once HONY is deposisted
		// Conditions: finder bee, no HONY, near the hive, has a direction to encode, direction % 2 == 0
		// (Meaning tell others the direction)
		if (parts[i].tmp2 == 1 && !parts[i].life && nearHive && parts[i].tmp && parts[i].tmp % 2 == 0)
		{
			int dis = parts[i].tmp / 1000;

			parts[i].vx -= 15.0f * std::cos(sim->frameCount / 3.0f);
			parts[i].vy -= 15.0f * std::sin(sim->frameCount / 3.0f);

			// Length of dance depends on distance
			if (sim->rng.chance(1, dis))
			{
				parts[i].tmp = 0;
			}
		}
		else
		{
			// Tend away from spawn if foraging, else towards
			// If full on pollen return to spawn
			// If give up return to spawn
			int m = parts[i].tmp2 == 1 && (parts[i].life < 100 || parts[i].tmp < 0) ? 1 : -1;
			if ((m < 0 && !nearHive) || (m > 0 && nearHive))
			{
				parts[i].vx += m * (parts[i].x - parts[i].tmp3) / 1500.0f;
				parts[i].vy += m * (parts[i].y - parts[i].tmp4) / 1500.0f;
			}
		}
	}

	// Restrict speed
	float maxSpeed = parts[i].tmp2 == 1 ? BEE_MAX_SPEED : BEE_MAX_BUILD_SPEED;
	parts[i].vx = restrict_flt(parts[i].vx, -maxSpeed, maxSpeed);
	parts[i].vy = restrict_flt(parts[i].vy, -maxSpeed, maxSpeed);

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	*pixel_mode |= NO_DECO;
	*colr *= ((float)cpart->dcolour + 1) / 6;
	*colg *= ((float)cpart->dcolour + 1) / 6;
	*colb *= ((float)cpart->dcolour + 1) / 6;

	// Queen bee is fatter by 1 px
	if (cpart->tmp2 == 2)
	{
		RGB col;
		col.Red = *colr;
		col.Green = *colg;
		col.Blue = *colb;
		gfctx.renderer->DrawPixel({ (int)cpart->x, (int)cpart->y }, col);
		gfctx.renderer->DrawPixel({ (int)cpart->x + 1, (int)cpart->y }, col);
	}
	return 0;
}
