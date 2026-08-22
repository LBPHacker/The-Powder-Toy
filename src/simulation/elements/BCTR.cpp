#include "simulation/ElementCommon.h"

constexpr int BCTR_TEMP_RES_MULTI = 5;
constexpr int BCTR_ENERGY_MULTI = 20;
constexpr int BCTR_FOOD_LIFE = 80;
constexpr int BCTR_TEMP_LIFE_GAIN = 5; // Energy gained per 0.5 degrees heat
constexpr int BCTR_START_LIFE = 110;
constexpr int BCTR_AGE_MULTI = 90;
constexpr int BCTR_AGE_BASE = 300;
constexpr int BCTR_MUTATION_RATE = 2; // Higher = less mutations

struct BCTRGenes
{
	int resType, resVal, foodType, metabolism, spread, move, graphics;
	bool glow;
};

/**
* Return an int represented by the bits of the target
* from start to end, for example if target was
* 000100 and we extracted from 0 to 3, we would get 0001 = 1
*
* Assumes 32-bit int
*/
static int extractBits(int target, int start, int end)
{
	if (!target)
	{
		return 0;
	}
	start = std::max(0, start);
	end = std::min(31, end);

	unsigned int mask = 0;
	for (auto t = start; t <= end; t++)
	{
		mask |= 1 << t;
	}
	return (target & mask) >> start;
}

static BCTRGenes extractGenes(int packedGenes)
{
	BCTRGenes genes;
	genes.resType = extractBits(packedGenes, 0, 3);
	genes.resVal = extractBits(packedGenes, 4, 7);
	genes.foodType = extractBits(packedGenes, 8, 11);
	genes.metabolism = extractBits(packedGenes, 12, 15);
	genes.spread = extractBits(packedGenes, 16, 19);
	genes.move = extractBits(packedGenes, 20, 22);
	genes.graphics = extractBits(packedGenes, 23, 25);
	genes.glow = extractBits(packedGenes, 26, 26);
	return genes;
}

/**
* Apply a random mutation to a target gene
*/
static int mutate(Simulation *sim, int gene)
{
	// Random flip a bit if mutation allows it
	if (sim->rng.chance(1, BCTR_MUTATION_RATE))
	{
		return gene;
	}
	return gene ^ (1 << sim->rng.between(0, 32));
}

/*
Bacteria features:
- Dies if it runs out of energy/food
- Dies if temperature is too high or low
- Dies if touches SOAP
- Infects STKM

Life stores energy value

tmp stores "age", time to life determined by metabolism: (15 - metabolism) * AGE_MULTI + AGE_BASE

tmp2 is if the bacteria is dead

Genes are stored in ctype
Also see GameView.cpp
4:  0000  - Resistance type (See below)
			1 = HEAT (and FIRE)
			2 = COLD
			3 = VIRS
			4 = SOAP / SALT
			5 = ACID
			6 = Being EATEN
			7 = Radiation
8:  0000  - Resistance value (0 - 15, 0 = no resistance)
12: 0000  - Food type (See below)
			(Not listed) = SUGR
			1 = YEST / DYST
			2 = PLNT / WOOD / SAWD
			3 = PHOT or BRAY
			4 = NEUT / ELEC / PROT
			5 = BCTR
			6 = Thermal energy
16: 0000  - Metabolism speed (0 - 15, 0 = no growth) not including other gene modifiers
	   	    Energy storage is inversely proportional to metabolism
20: 0000  - Spread vector
			(Not listed) - Default liquid
			1 - Stick to solids and powders
			2 - Float through the air
			3 - Randomly swap with liquid particles
			4 - Inject self DNA into other BCTR
23: 000   - Move speed (increased energy cost, 0 - 7)
26: 000   - Graphics (fuzziness?)
			(Not listed) - PMODE_BLUR
			1 - SPARK
			2 - BLOB
			3 - FLAT
27: 0     - Glow in the dark?
31: 00000 - Unused


Will pass on dcolour to children

Genes:
- Resistances:
   - Heat / Cold
   - Radiation
   - VIRS
   - SOAP / ACID
   - Other BCTR
- Metabolize different food sources
   - SUGR (default)
   - YEST (not that far of an evolutionary leap)
   - PLNT / photosynthesis
   - Other BCTR?
- Growth speed
   - How fast it consumes energy
   - Produces gas as it consumes (depends on food)
- Energy storage capacity
   - Spends energy to store however
- Spread vector
   - Stick to solids
   - Spread through air
   - Spread through liquids
   - dissolve into solution. WATR (ctype BCTR)
- Color
   - Fancy colors ooooh
   - Glow in the dark gene, costs energy but is cool

Multiplier for speed of growth
Bacteria requires less sugar to grow as time goes on or if it has enough food
 */

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_BCTR()
{
	Identifier = "DEFAULT_PT_BCTR";
	Name = "BCTR";
	Colour = 0xDCF781_rgb;
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

	Weight = 30;

	HeatConduct = 150;
	Description = "Bacteria. Feed it sugar to grow, can evolve genes.";

	Properties = TYPE_LIQUID | PROP_DEADLY | PROP_NEUTPENETRATE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 200.0f + 273.15f;
	HighTemperatureTransition = PT_FIRE;

	Update = &update;
	Graphics = &graphics;

	DefaultProperties.life = BCTR_START_LIFE;
}

static int update(UPDATE_FUNC_ARGS)
{
	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;

	auto eat = [sim, i, &parts](int r, int xr, int yr, int other) {
		if (other != PT_NONE && sim->rng.chance(1, 30))
		{
			sim->part_change_type(ID(r), xr, yr, other);
		}
		else
		{
			sim->kill_part(ID(r));
		}
		parts[i].life += BCTR_FOOD_LIFE;
	};

	// Dead, dissolvable by SOAP
	if (parts[i].tmp2)
	{
		if (parts[i].temp < 273.15f)
		{
			sim->part_change_type(i, x, y, PT_ICEI);
			parts[i].ctype = PT_BCTR;
			return 1;
		}

		for (auto rx = -1; rx <= 1; rx++)
		{
			for (auto ry = -1; ry <= 1; ry++)
			{
				auto r = pmap[y + ry][x + rx];

				// Burn
				if (TYP(r) == PT_FIRE || TYP(r) == PT_PLSM)
				{
					sim->part_change_type(i, x, y, PT_FIRE);
					parts[i].life = sim->rng.between(0, 200);
					parts[i].temp += 50.0f;
					return 1;
				}

				if ((TYP(r) == PT_SALT || TYP(r) == PT_SOAP) && sim->rng.chance(1, 100))
				{
					sim->kill_part(i);
					return 1;
				}
			}
		}

		return 0;
	}

	// If all genes somehow become 1 become sing :D
	if (parts[i].ctype == ~0)
	{
		sim->part_change_type(i, x, y, PT_SING);
		return 1;
	}

	auto [
		resType,
		resVal,
		foodType,
		metabolism,
		spread,
		move,
		graphics,
		glow
	] = extractGenes(parts[i].ctype);

	// Temp line
	metabolism = 7;

	int energyCapcity = 2 * BCTR_START_LIFE + (15 - metabolism) * BCTR_ENERGY_MULTI;

	// Inc age, randomize slightly
	if (sim->rng.chance(19, 20))
	{
		parts[i].tmp++;
	}

	// Utilize energy
	if (sim->frameCount % 50 == 0)
	{
		parts[i].life -= metabolism + move + glow + 1; // Can't have 0 energy consumption
	}

	// Die if no food
	if (parts[i].life <= 0)
	{
		parts[i].life = 0;
		parts[i].tmp2 = 1;
		return 0;
	}

	// Too old
	if (parts[i].tmp >= (15 - metabolism) * BCTR_AGE_MULTI + BCTR_AGE_BASE)
	{
		// Replace self with new cell if enough energy
		if (parts[i].life >= 2 * BCTR_START_LIFE)
		{
			parts[i].life -= BCTR_START_LIFE;
			parts[i].ctype = mutate(sim, parts[i].ctype);
			parts[i].tmp = 0;
		}
		else
		{
			parts[i].tmp2 = 1;
		}

		return 0;
	}

	// Temperature
	float maxTemp = 273.15f + 70.0f + BCTR_TEMP_RES_MULTI * (resType == 1) * resVal;
	if (parts[i].temp > maxTemp)
	{
		parts[i].tmp2 = 1;
		return 0;
	}
	float minTemp = 273.15f - BCTR_TEMP_RES_MULTI * (resType == 2) * resVal;
	if (parts[i].temp < minTemp)
	{
		sim->part_change_type(i, x, y, PT_ICEI);
		parts[i].ctype = PT_BCTR;
		parts[i].tmp2 = 1;
		return 1;
	}

	for (auto rx = -1; rx <= 1; rx++)
	{
		for (auto ry = -1; ry <= 1; ry++)
		{
			if (rx || ry)
			{
				auto r = pmap[y + ry][x + rx];
				auto rt = TYP(r);

				// Burn
				if ((rt == PT_FIRE || rt == PT_PLSM) && sim->rng.chance(1, 80) && (resType != 1 || resVal < 8))
				{
					sim->part_change_type(i, x, y, PT_FIRE);
					parts[i].life = sim->rng.between(0, 100);
					parts[i].temp += 30.0f;
					return 1;
				}

				// Eat other bacteria
				if (foodType == 5 && TYP(r) == PT_BCTR && sim->rng.chance(1, 200) == 1)
				{
					BCTRGenes other = extractGenes(parts[ID(r)].ctype);
					if (other.resType != 6 || (other.resType == 6 && sim->rng.between(0, 15) > other.resVal))
					{
						// Glow gene is always passed
						if (other.glow)
						{
							parts[ID(i)].ctype |= 1 << 26;
						}
						eat(r, x + rx, y + ry, PT_NONE);
						return 0;
					}
				}

				// Movement
				if (r && move == 1 && (elements[rt].Properties & TYPE_SOLID || elements[rt].Properties & TYPE_PART)) // 1 = stick to solids and powders
				{
					parts[i].vx = parts[i].vy = 0.0f;
				}
				else if (!r && move == 2 && sim->rng.chance(move, 20)) // 2 = Float through the air
				{
					parts[i].vx += rx;
					parts[i].vy += ry;
				}
				else if (r && move == 3 && elements[rt].Properties & TYPE_LIQUID && sim->rng.chance(move, 20)) // 3 = Swap with liquid particles
				{
					auto nxf = parts[ID(r)].x;
					auto nyf = parts[ID(r)].y;
					sim->move(ID(r), x + rx, y + ry, parts[i].x, parts[i].y);
					if (!sim->move(i, x, y, nxf, nyf))
					{
						return 1;
					}
					return 0;
				}
				else if (r && move == 4 && TYP(r) == PT_BCTR && parts[i].life > BCTR_START_LIFE && sim->rng.chance(1, 100)) // 4 = Reproduce by injecting DNA into other BCTR
				{
					parts[i].life -= BCTR_START_LIFE;
					parts[i].ctype = mutate(sim, parts[i].ctype);
					parts[ID(r)].ctype = parts[i].ctype;
					return 0;
				}

				if ((!r || TYP(r) == PT_BCTR) && move != 4)
				{
					// Reproduce if enough energy stored and spot is empty
					if (parts[i].life >= 2 * BCTR_START_LIFE)
					{
						parts[i].life -= BCTR_START_LIFE;

						auto j = r >= 0 ? ID(r) : sim->create_part(-1, x + rx, y + ry, PT_BCTR);
						if (j >= 0)
						{
							parts[j].ctype = mutate(sim, parts[i].ctype);
							parts[j].dcolour = parts[i].dcolour;
						}
					}
					r = sim->photons[y + ry][x + rx];
				}
				if (!r)
				{
					continue;
				}
				rt = TYP(r);

				// Consume food, boost energy
				// Metabolism is % chance of eating / 5
				if (parts[i].life < energyCapcity && sim->rng.between(0, 500) < metabolism)
				{
					if (
						(foodType == 1 && (rt == PT_YEST || rt == PT_DYST)) ||
						(foodType == 2 && (rt == PT_WOOD || rt == PT_PLNT || rt == PT_SAWD ||
							((rt == PT_FLSH || rt == PT_POTO || rt == PT_STMH || rt == PT_UDDR) && sim->rng.chance(1, 50))
						)) ||
						(foodType == 4 && (rt == PT_NEUT || rt == PT_PROT))
					)
					{
						eat(r, x + rx, y + ry, PT_GAS);
						return 0;
					}
					else if (foodType == 3 && (rt == PT_PHOT || rt == PT_BRAY))
					{
						parts[i].temp += 2.0f;
						eat(r, x + rx, y + ry, PT_NONE);
						return 0;
					}
					else if (foodType == 6 && r && parts[i].temp < parts[ID(r)].temp) // Absorb thermal energy
					{
						parts[i].life += BCTR_TEMP_LIFE_GAIN;
						parts[i].temp += 0.5f;
						parts[ID(r)].temp -= 0.5f;
					}
					else if ((foodType < 1 || foodType > 6) && (TYP(r) == PT_SWTR || TYP(r) == PT_SUGR))
					{
						eat(r, x + rx, y + ry, PT_GAS);
						return 0;
					}
				}

				// Determine resistance against other elements
				bool shouldRes = sim->rng.between(0, 15) <= resVal;
				if (resType == 3 && shouldRes && (rt == PT_VRSS || rt == PT_VRSG || rt == PT_VIRS))
				{
					parts[ID(r)].tmp3 = 1;
				}
				else if (resType == 5 && shouldRes && rt == PT_ACID)
				{
					sim->kill_part(ID(r));
					return 0;
				}
				else if ((resType != 4 || !shouldRes) && sim->rng.chance(1, 10) && (rt == PT_SALT || rt == PT_SOAP))
				{
					parts[i].tmp2 = 1;
					return 0;
				}
				else if ((resType != 7 || !shouldRes) && sim->rng.chance(1, 10) && (rt == PT_PROT || rt == PT_NEUT || rt == PT_ELEC)) // Radiation causes mutations, resistance decreases change
				{
					parts[i].ctype = mutate(sim, parts[i].ctype);
				}
				else if (rt == PT_LEAD || rt == PT_BRAS || rt == PT_SUFR || rt == PT_BRNZ) // Dies when touching LEAD or BRAS or SUFR
				{
					parts[i].tmp2 = 1;
					return 0;
				}
			}
		}
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	*pixel_mode |= PMODE_BLUR;

	// Dead
	if (cpart->tmp2)
	{
		*colr *= 0.8; *colg *= 0.8; *colb *= 0.8;
		*colr += gfctx.rng.between(-10, 10);
		*colg += gfctx.rng.between(-10, 10);
		*colb += gfctx.rng.between(-10, 10);

		return 0;
	}

	// Color vary a bit depending on mutation
	*colr = (*colr + (cpart->ctype ^ 9999) % 255) / 2;
	*colg = (*colg + 255 - *colr) / 2;
	*colb = (*colb + 255 - (cpart->ctype ^ 999999) % 255) / 2;

	// Graphical effects
	int graphics = extractBits(cpart->ctype, 23, 25);
	if (graphics == 1)
	{
		*pixel_mode = PMODE_SPARK;
	}
	else if (graphics == 2)
	{
		*pixel_mode = PMODE_BLOB;
	}
	else if (graphics == 3)
	{
		*pixel_mode = PMODE_FLAT;
	}

	// Glow in the dark
	int glow = extractBits(cpart->ctype, 26, 26);
	if (glow)
	{
		*firer = *colr;
		*fireg = *colg;
		*fireb = *colb;
		*firea = 240;
		*pixel_mode |= PMODE_GLOW;
	}

	return 0;
}
