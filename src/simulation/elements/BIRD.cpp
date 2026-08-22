#include "simulation/ElementCommon.h"

constexpr static int BIRD_SEARCH_RANGE = 5;
constexpr static int BIRD_AVOID_RANGE = 2;
constexpr static float BIRD_MAX_VELOCITY = 0.8f;
constexpr static float BIRD_ACCELERATION = 0.05f;
constexpr static float BIRD_AVOID_TEMP = 90.0f + 273.15f;

constexpr static int BIRD_EDGE_RANGE = 10;
constexpr static float BIRD_EDGE_PUSH = 200.0f;

static int update(UPDATE_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_BIRD()
{
	Identifier = "DEFAULT_PT_BIRD";
	Name = "BIRD";
	Colour = 0xB8A174_rgb;
	MenuVisible = 1;
	MenuSection = SC_ORGANIC;
	Enabled = 1;

	Advection = 0.1f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.94f;
	Loss = 1.00f;
	Collision = -0.1f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 30;
	Explosive = 1;
	Meltable = 0;
	Hardness = 2;

	Weight = 20;

	HeatConduct = 150;
	Description = "Birds, flies in flocks, eats ants, fish, and seeds. Flammable.";

	Properties = TYPE_PART | PROP_EDIBLE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = 6.0f;
	HighPressureTransition = PT_BLOD;
	LowTemperature = -20.0f + 273.15f;
	LowTemperatureTransition = PT_DUST;
	HighTemperature = 90.0f + 273.15f;
	HighTemperatureTransition = PT_DUST;

	FoodValue = 2;

	Update = &update;

	Create = &create;
}

static int update(UPDATE_FUNC_ARGS)
{
	/** Properties:
	 * tmp - is perching?
	 * tmp2 - perch timer
	 * life - want to perch?
	 * tmp3 and tmp4: spawn location
	 *
	 * Behavior:
	 * - Birds flock
	 * - Randomly land onto solids
	 * - Avoid fire, pressure, heat, lava, anything PROP_DEADLY
	 * - Head towards ANT and SEED
	 * - If edge mode is WALL or VOID, avoid edges
	 */

	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;

	int birds = 0;
	int obstacles = 0;
	bool objectDetected = false;
	bool scatter = false;
	bool wantToPerch = parts[i].life > 0;

	// Stop perching
	if (parts[i].tmp2 < 0)
	{
		parts[i].life = 0;
		parts[i].tmp = 0;
		parts[i].tmp2 = 0;
	}

	// Perch countdown, freeze
	if (parts[i].tmp)
	{
		parts[i].life = 0;
		parts[i].vx = parts[i].vy = 0.0f;
		parts[i].tmp2--;

		// Check if there is still a solid nearby to perch on
		for (auto rx = -1; rx <= 1; rx++)
		{
			for (auto ry = -1; ry <= 1; ry++)
			{
				if (rx || ry)
				{
					auto r = pmap[y + ry][x + rx];
					if (r && elements[TYP(r)].Properties & TYPE_SOLID)
					{
						return 0;
					}
				}
			}
		}

		// Stop perching, nothing to perch on
		parts[i].tmp2 = -1;
		return 0;
	}

	if (sim->rng.chance(1, 600))
	{
		parts[i].life = 1;
	}

	Vec2<float> flockVel{ 0.0f, 0.0f };
	Vec2<float> center{ 0.0f, 0.0f };
	Vec2<float> repulse{ 0.0f, 0.0f };
	Vec2<float> repulseObject{ 0.0f, 0.0f };

	// Avoid edges.
	if (x < BIRD_EDGE_RANGE)
	{
		repulseObject.X += BIRD_EDGE_PUSH;
	}
	else if (x > XRES - BIRD_EDGE_RANGE)
	{
		repulseObject.X -= BIRD_EDGE_PUSH;
	}

	if (y < BIRD_EDGE_RANGE)
	{
		repulseObject.Y += BIRD_EDGE_PUSH;
	}
	else if (y > YRES - BIRD_EDGE_RANGE)
	{
		repulseObject.Y -= BIRD_EDGE_PUSH;
	}

	if (repulseObject.X || repulseObject.Y)
	{
		objectDetected = true;
		obstacles = 1;
	}

	for (auto rx = -BIRD_SEARCH_RANGE; rx <= BIRD_SEARCH_RANGE; rx++)
	{
		for (auto ry = -BIRD_SEARCH_RANGE; ry <= BIRD_SEARCH_RANGE; ry++)
		{
			if (rx || ry)
			{
				auto r = pmap[y + ry][x + rx];
				if (!r)
				{
					continue;
				}
				auto rt = TYP(r);

				if (rt == PT_BIRD)
				{
					// Tend towards flock direction
					flockVel.X += parts[ID(r)].vx;
					flockVel.Y += parts[ID(r)].vy;

					center.X += parts[ID(r)].x;
					center.Y += parts[ID(r)].y;
					birds++;
					objectDetected = true;

					// Tend away from nearby birds, the closer they are the stronger the repulsion
					if (std::abs(rx) <= BIRD_AVOID_RANGE && std::abs(ry) <= BIRD_AVOID_RANGE)
					{
						repulse.X -= rx;
						repulse.Y -= ry;
					}
					continue;
				}
				else if (rt == PT_ANT || rt == PT_SEED || rt == PT_FISH) // Tend towards food
				{
					repulseObject.X += rx;
					repulseObject.Y += ry;
					obstacles++;
					objectDetected = true;
				}
				else if (
					(parts[i].temp > BIRD_AVOID_TEMP || !(elements[rt].Properties & TYPE_GAS) || sim->IsWallBlocking(x + rx, y + ry, PT_BIRD)) &&
					rt != PT_WEB && rt != PT_GLAS && rt != PT_TRBN
				) // Tend away from obstacles detected. Anything that's not a gas or too hot is an obstacle. Birds can't see glass though
				{
					repulseObject.X -= rx;
					repulseObject.Y -= ry;
					obstacles++;
					objectDetected = true;

					// If birds see anything that's PROP_DEADLY or hot scatter
					if (elements[rt].Properties & PROP_DEADLY || parts[i].temp > BIRD_AVOID_TEMP || rt == PT_LIGH || rt == PT_BOMB)
					{
						scatter = true;
					}

					// If want to perch attract instead to solids
					if (wantToPerch && elements[rt].Properties & TYPE_SOLID && rt != PT_GLAS && rt != PT_WEB && rt != PT_TRBN)
					{
						repulseObject.X += 2 * rx;
						repulseObject.Y += 2 * ry;
					}
				}

				// Collisions
				if (std::abs(rx) < 2 && std::abs(ry) < 2)
				{
					if (rt == PT_GLAS && std::hypot(parts[i].vx, parts[i].vy) > 0.5f) // Die upon colliding with glass
					{
						sim->part_change_type(i, x, y, PT_BLOD);
						return 0;
					}
					else if (rt == PT_SPRK || rt == PT_LIGH) // Fried by electricity
					{
						sim->part_change_type(i, x, y, PT_DUST);
						parts[i].temp += 100.0f;
						return 0;
					}
					else if (rt == PT_BIRD && sim->rng.chance(1, 3)) // Stop perching if crowded
					{
						parts[i].tmp2 = -1;
						return 0;
					}
					else if (wantToPerch && elements[rt].Properties & TYPE_SOLID) // Perch timer
					{
						parts[i].tmp = 1;
						parts[i].tmp2 = sim->rng.between(50, 200);
						return 0;
					}
					else if ((rt == PT_SEED || rt == PT_ANT || rt == PT_FISH) && sim->rng.chance(1, 100)) // Eat food
					{
						sim->kill_part(ID(r));
						return 0;
					}
				}
			}
		}
	}

	if (objectDetected)
	{
		if (obstacles)
		{
			// Avoid obstacles
			parts[i].vx += repulseObject.X / obstacles;
			parts[i].vy += repulseObject.Y / obstacles;
		}
		else if (birds)
		{
			// Avoid being too close to other birds
			parts[i].vx += repulse.X / 3.0f / birds;
			parts[i].vy += repulse.Y / 3.0f / birds;

			// Follow center of flock (If scatter is true do the opposite)
			if (!scatter)
			{
				parts[i].vx += (center.X / birds - parts[i].x) / 100.0f;
				parts[i].vy += (center.Y / birds - parts[i].y) / 100.0f;
			}
			else
			{
				parts[i].vx -= (center.X / birds - parts[i].x) / 100.0f;
				parts[i].vy -= (center.Y / birds - parts[i].y) / 100.0f;
			}

			// Match nearby velocities
			parts[i].vx += (flockVel.X / birds - parts[i].vx) / 8;
			parts[i].vy += (flockVel.Y / birds - parts[i].vy) / 8;
		}
	}

	// Randomize dir if alone or not moving (and no obstacle to avoid)
	if (sim->rng.chance(1, 50) || (!objectDetected && parts[i].vx + parts[i].vy < 0.2f))
	{
		float angle = sim->rng.uniform01() * 2 * std::numbers::pi_v<float>;
		parts[i].vx += std::cos(angle) * BIRD_ACCELERATION;
		parts[i].vy += std::sin(angle) * BIRD_ACCELERATION;
	}

	// Tend towards spawn location
	// parts[i].vx += (parts[i].tmp3 - parts[i].x) / 40000.0f;
	// parts[i].vy += (parts[i].tmp4 - parts[i].y) / 40000.0f;

	// Limit velocity
	parts[i].vx = restrict_flt(parts[i].vx, -BIRD_MAX_VELOCITY, BIRD_MAX_VELOCITY);
	parts[i].vy = restrict_flt(parts[i].vy, -BIRD_MAX_VELOCITY, BIRD_MAX_VELOCITY);

	return 0;
}

static void create(ELEMENT_CREATE_FUNC_ARGS)
{
	// Randomize velocity
	float angle = sim->rng.uniform01() * 2 * std::numbers::pi_v<float>;
	sim->parts[i].vx = std::cos(angle);
	sim->parts[i].vy = std::sin(angle);

	// Store spawn location
	sim->parts[i].tmp3 = sim->parts[i].x;
	sim->parts[i].tmp4 = sim->parts[i].y;
}
