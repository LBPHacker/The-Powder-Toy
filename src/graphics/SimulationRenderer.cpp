#include "SimulationRenderer.h"

#include "simulation/Air.h"
#include "simulation/Gravity.h"
#include "simulation/Sign.h"
#include "simulation/Simulation.h"
#include "simulation/SimulationData.h"
#include "simulation/ElementGraphics.h"
#include "simulation/ElementClasses.h"
#include "Misc.h"
#include "common/tpt-rand.h"
#include "graphics/Pix.h"
#include "common/Line.h"
#include "CommonShapes.h"

#include "hmap.h"
#include "FontData.h"

#include <algorithm>
#include <cmath>

SimulationRenderer::SimulationRenderer() :
	mainBuffer(XRES * YRES, 0),
	persBuffer(XRES * YRES, 0),
	warpBuffer(XRES * YRES, 0),
	fire((YRES / CELL) * (XRES / CELL), 0),
	gravityFieldEnabled(false),
	gravityZonesEnabled(false),
	decorationsEnabled(true),
	blackDecorations(false),
	colorMode(0),
	displayMode(0),
	renderMode(RENDER_BASC | PMODE_BLOB),
	gridSize(0),
	graphicsCache(PT_NUM)
{
	InitGraphicsTables();
}

template<bool Checked>
uint32_t SimulationRenderer::GetPixel(int x, int y)
{
	if (Checked && (x < 0 || y < 0 || x >= XRES || y >= YRES))
	{
		return 0;
	}
	return mainBuffer[y * XRES + x];
}

template<bool Checked>
void SimulationRenderer::SetPixel(int x, int y, uint32_t rgb)
{
	if (Checked && (x < 0 || y < 0 || x >= XRES || y >= YRES))
	{
		return;
	}
	mainBuffer[y * XRES + x] = rgb;
}

template<bool Checked>
void SimulationRenderer::BlendPixel(int x, int y, uint32_t rgba)
{
	auto rgb = GetPixel<Checked>(x, y);
	auto a = PixA(rgba);
	if (a != 255U)
	{
		uint32_t r = (a * PixR(rgba) + (255U - a) * PixR(rgb)) >> 8;
		uint32_t g = (a * PixG(rgba) + (255U - a) * PixG(rgb)) >> 8;
		uint32_t b = (a * PixB(rgba) + (255U - a) * PixB(rgb)) >> 8;
		rgba = PixRGB(r, g, b);
	}
	SetPixel<Checked>(x, y, rgba);
}

template<bool Checked>
void SimulationRenderer::AddPixel(int x, int y, uint32_t rgba)
{
	auto rgb = GetPixel<Checked>(x, y);
	auto a = PixA(rgba);
	uint32_t r = (a * PixR(rgba) + 255U * PixR(rgb)) >> 8;
	uint32_t g = (a * PixG(rgba) + 255U * PixG(rgb)) >> 8;
	uint32_t b = (a * PixB(rgba) + 255U * PixB(rgb)) >> 8;
	if (r > 255U) r = 255U;
	if (g > 255U) g = 255U;
	if (b > 255U) b = 255U;
	SetPixel<Checked>(x, y, PixRGB(r, g, b));
}

void SimulationRenderer::DrawBlob(int x, int y, uint32_t rgba)
{
	auto half = PixRGBA(PixR(rgba), PixG(rgba), PixB(rgba), PixA(rgba) / 2);
	BlendPixel<true>(x - 1, y - 1, half);
	BlendPixel<true>(x    , y - 1, rgba);
	BlendPixel<true>(x + 1, y - 1, half);
	BlendPixel<true>(x - 1, y    , rgba);
	BlendPixel<true>(x + 1, y    , rgba);
	BlendPixel<true>(x - 1, y + 1, half);
	BlendPixel<true>(x    , y + 1, rgba);
	BlendPixel<true>(x + 1, y + 1, half);
}

void SimulationRenderer::DrawLine(int x0, int y0, int x1, int y1, uint32_t rgb)
{
	Line(x0, y0, x1, y1, [this, rgb](int x, int y) {
		SetPixel<true>(x, y, rgb);
	});
}

void SimulationRenderer::DrawStickman(int x, int y, int t, const Simulation &simulation, const playerst &stickman, uint32_t headrgb)
{
	uint32_t legrgb = headrgb;
	if (!(colorMode & COLOUR_HEAT))
	{
		headrgb = PixRGB(0x80, 0x80, 0xFF);
		if (!stickman.fan && stickman.elem < PT_NUM && stickman.elem > 0)
		{
			headrgb = simulation.elements[stickman.elem].Colour;
		}
		legrgb = t == PT_STKM2 ? PixRGB(0x64, 0x64, 0xFF) : PixRGB(0xFF, 0xFF, 0xFF);
	}
	if (t == PT_FIGH)
	{
		DrawLine(x    , y + 2, x + 2, y    , headrgb);
		DrawLine(x + 2, y    , x    , y - 2, headrgb);
		DrawLine(x    , y - 2, x - 2, y    , headrgb);
		DrawLine(x - 2, y    , x    , y + 2, headrgb);
	}
	else
	{
		DrawLine(x - 2, y + 2, x + 2, y + 2, headrgb);
		DrawLine(x - 2, y - 2, x + 2, y - 2, headrgb);
		DrawLine(x - 2, y - 2, x - 2, y + 2, headrgb);
		DrawLine(x + 2, y - 2, x + 2, y + 2, headrgb);
	}
	auto &legs = stickman.legs;
	DrawLine(            x,         y + 3, int(legs[ 0]), int(legs[ 1]), legrgb);
	DrawLine(int(legs[ 0]), int(legs[ 1]), int(legs[ 4]), int(legs[ 5]), legrgb);
	DrawLine(            x,         y + 3, int(legs[ 8]), int(legs[ 9]), legrgb);
	DrawLine(int(legs[ 8]), int(legs[ 9]), int(legs[12]), int(legs[13]), legrgb);
	if (stickman.rocketBoots)
	{
		for (int leg = 0; leg < 2; ++leg)
		{
			auto lx = int(legs[leg * 8 + 4]);
			auto ly = int(legs[leg * 8 + 5]);
			auto comm = int(stickman.comm);
			if ((comm & 4) || ((comm & 1) && !leg) || ((comm & 2) && leg))
			{
				BlendPixel<true>(lx, ly, PixRGBA(0, 255, 0, 255));
			}
			else
			{
				BlendPixel<true>(lx, ly, PixRGBA(255, 0, 0, 255));
			}
			DrawBlob(lx, ly, PixRGBA(255, 0, 255, 223));
		}
	}
}

void SimulationRenderer::RenderPersistent()
{
	if (displayMode & DISPLAY_PERS)
	{
		std::swap(mainBuffer, persBuffer);
	}
	else
	{
		std::fill(mainBuffer.begin(), mainBuffer.end(), 0);
	}
}

void SimulationRenderer::RenderAir(const Simulation &simulation)
{
	if (!(displayMode & DISPLAY_AIR))
	{
		return;
	}
	auto *pv = simulation.air->pv;
	auto *hv = simulation.air->hv;
	auto *vx = simulation.air->vx;
	auto *vy = simulation.air->vy;

	// * The loop should be replaced with a generic lambda once we hit C++14.
	for (int y = 0; y < YRES / CELL; ++y)
	{
		for (int x = 0; x < XRES / CELL; ++x)
		{
			int r = 0;
			int g = 0;
			int b = 0;
			// * TODO-REDO_UI: These are mutually exclusive; why are they bits?
			if (displayMode & DISPLAY_AIRP)
			{
				if (pv[y][x] > 0.0f)
				{
					r = clamp_flt(pv[y][x], 0.0f, 8.0f);
					g = 0;
					b = 0;
				}
				else
				{
					r = 0;
					g = 0;
					b = clamp_flt(-pv[y][x], 0.0f, 8.0f);
				}
			}
			else if (displayMode & DISPLAY_AIRV)
			{
				r = clamp_flt(fabsf(vx[y][x]), 0.0f, 8.0f);
				g = clamp_flt(      pv[y][x] , 0.0f, 8.0f); // * No fabsf?
				b = clamp_flt(fabsf(vy[y][x]), 0.0f, 8.0f);
			}
			else if (displayMode & DISPLAY_AIRH)
			{
				// * These really shouldn't be macros.
				const float temp_min = MIN_TEMP;
				const float temp_max = MAX_TEMP;
				auto caddress = (int)((hv[y][x] - temp_min) / (temp_max - temp_min) * 1024);
				if (caddress <    0) caddress =    0;
				if (caddress > 1023) caddress = 1023;
				r = int(color_data[caddress * 3    ] * 0.7f);
				g = int(color_data[caddress * 3 + 1] * 0.7f);
				b = int(color_data[caddress * 3 + 2] * 0.7f);
			}
			else if (displayMode & DISPLAY_AIRC)
			{
				r = clamp_flt(fabsf(vx[y][x]), 0.0f, 24.0f) + clamp_flt(fabsf(vy[y][x]), 0.0f, 20.0f);
				g = clamp_flt(fabsf(vx[y][x]), 0.0f, 20.0f) + clamp_flt(fabsf(vy[y][x]), 0.0f, 24.0f);
				b = clamp_flt(fabsf(vx[y][x]), 0.0f, 24.0f) + clamp_flt(fabsf(vy[y][x]), 0.0f, 20.0f);
				if (pv[y][x] > 0.0f)
				{
					r += clamp_flt(pv[y][x], 0.0f, 8.0f);
				}
				else
				{
					b += clamp_flt(-pv[y][x], 0.0f, 8.0f);
				}
			}
			for (int j = 0; j < CELL; ++j)
			{
				for (int i = 0; i < CELL; ++i)
				{
					SetPixel<false>(x * CELL + i, y * CELL + j, PixRGB(r, g, b));
				}
			}
		}
	}
}

void SimulationRenderer::RenderGravity(const Simulation &simulation)
{
	if (gravityFieldEnabled)
	{
		for (int y = 0; y < YRES / CELL; ++y)
		{
			for (int x = 0; x < XRES / CELL; ++x)
			{
				int ca = y * (XRES / CELL) + x;
				auto gx = fabsf(simulation.gravx[ca]);
				auto gy = fabsf(simulation.gravy[ca]);
				if (gx <= 0.001f && gy <= 0.001f)
				{
					continue;
				}
				int nx = x * CELL;
				int ny = y * CELL;
				auto dist = gx + gy;
				for (int i = 0; i < 4; ++i)
				{
					auto cx = (int)((nx - simulation.gravx[ca] * 0.5f) + 0.5f);
					auto cy = (int)((ny - simulation.gravy[ca] * 0.5f) + 0.5f);
					AddPixel<true>(cx, cy, PixRGBA(255, 255, 255, uint8_t(dist * 20.0f)));
				}
			}
		}
	}
}

void SimulationRenderer::RenderWalls(const Simulation &simulation)
{
	for (int y = 0; y < YRES / CELL; ++y)
	{
		for (int x = 0; x < XRES / CELL; ++x)
		{
			if (simulation.bmap[y][x])
			{
				unsigned char wt = simulation.bmap[y][x];
				if (wt >= UI_WALLCOUNT)
				{
					continue;
				}
				unsigned char powered = simulation.emap[y][x];
				uint32_t pc = simulation.wtypes[wt].colour;
				uint32_t gc = simulation.wtypes[wt].eglow;
				auto drawstyle = simulation.wtypes[wt].drawstyle;

				if (wt == WL_STREAM)
				{
					float xf = x * CELL + CELL * 0.5f;
					float yf = y * CELL + CELL * 0.5f;
					int oldX = (int)(xf + 0.5f);
					int oldY = (int)(yf + 0.5f);
					int newX, newY;
					float xVel = simulation.vx[y][x] * 0.125f;
					float yVel = simulation.vy[y][x] * 0.125f;
					if (xVel || yVel)
					{
						bool changed = false;
						for (int t = 0; t < 1024; ++t)
						{
							newX = (int)(xf + 0.5f);
							newY = (int)(yf + 0.5f);
							if (newX != oldX || newY != oldY)
							{
								changed = true;
								oldX = newX;
								oldY = newY;
							}
							if (changed && (newX < 0 || newX >= XRES || newY < 0 || newY >= YRES))
							{
								break;
							}
							AddPixel<true>(newX, newY, PixRGBA(255, 255, 255, 64));
							if (changed)
							{
								int wallX = newX / CELL;
								int wallY = newY / CELL;
								xVel = simulation.vx[wallY][wallX] * 0.125f;
								yVel = simulation.vy[wallY][wallX] * 0.125f;
								if (wallX != x && wallY != y && simulation.bmap[wallY][wallX] == WL_STREAM)
								{
									break;
								}
							}
							xf += xVel;
							yf += yVel;
						}
					}
					else
					{
						AddPixel<true>(oldX, oldY, PixRGBA(255, 255, 255, 255));
					}
					StreamBase(x * CELL + 2, y * CELL + 2, [this](int x, int y, int c) {
						BlendPixel<true>(x, y, c);
					});
				}

				Wall(x, y, powered, drawstyle, pc, gc, [this](int x, int y, int c) {
					SetPixel<false>(x, y, c);
					if ((renderMode & PMODE_BLOB) && c != 0x202020)
					{
						DrawBlob(x, y, PixRGBA(PixR(c), PixG(c), PixB(c), 120));
					}
				});

				if (gc && powered)
				{
					fire[y * (XRES / CELL) + x] = gc;
				}
			}
		}
	}
}

void SimulationRenderer::RenderGrid()
{
	if (gridSize)
	{
		for (auto y = 0U; y < YRES; ++y)
		{
			for (auto x = 0U; x < XRES; ++x)
			{
				if (!(x % (CELL * gridSize)) || !(y % (CELL * gridSize)))
				{
					BlendPixel<false>(x, y, PixRGBA(100, 100, 100, 80));
				}
			}
		}
	}
}

void SimulationRenderer::RenderParticles(const Simulation &simulation)
{
	auto *parts = simulation.parts;
	auto *photons = simulation.photons;
	auto *elements = simulation.elements.data();
	for (int i = 0; i <= simulation.parts_lastActiveIndex; ++i)
	{
		auto &part = parts[i];
		int t = part.type;
		if (!t || t < 0 || t >= PT_NUM)
		{
			continue;
		}
		auto &elem = elements[t];

		auto nx = int(part.x + 0.5f);
		auto ny = int(part.y + 0.5f);
		if (nx < 0 || ny < 0 || nx >= XRES || ny >= YRES)
		{
			continue;
		}
		if (TYP(photons[ny][nx]) && !(elem.Properties & TYPE_ENERGY || t == PT_STKM || t == PT_STKM2 || t == PT_FIGH))
		{
			continue;
		}

		int decA = (part.dcolour >> 24) & 0xFF;
		int decR = (part.dcolour >> 16) & 0xFF;
		int decG = (part.dcolour >>  8) & 0xFF;
		int decB = (part.dcolour      ) & 0xFF;
		if (decorationsEnabled && blackDecorations)
		{
			if (decA < 250 || decR > 5 || decG > 5 || decB > 5)
			{
				decA = 0;
			}
			else
			{
				decA = 255;
				decR = 0;
				decG = 0;
				decB = 0;
			}
		}

		SimulationGraphicsValues gv;
		gv.pixelMode = PMODE_FLAT;
		gv.colA = 255;
		gv.colR = PixR(elem.Colour);
		gv.colG = PixG(elem.Colour);
		gv.colB = PixB(elem.Colour);
		gv.firA = 0;
		gv.firR = 0;
		gv.firG = 0;
		gv.firB = 0;
		if (!(colorMode & COLOUR_BASC))
		{
			if (graphicsCache[t].ready)
			{
				gv = graphicsCache[t];
			}
			else
			{
				if (elements[t].Graphics)
				{
					// * TODO-REDO_UI: call lua graphics functions through a wrapper
					gv.ready = elements[t].Graphics(
						this,
						&simulation,
						&part,
						nx,
						ny,
						&gv.pixelMode,
						&gv.colA,
						&gv.colR,
						&gv.colG,
						&gv.colB,
						&gv.firA,
						&gv.firR,
						&gv.firG,
						&gv.firB
					);
				}
				else
				{
					gv.ready = true;
				}
				if (gv.ready)
				{
					graphicsCache[t] = gv;
				}
			}
		}

		if((gv.pixelMode & FIRE_ADD) && !(renderMode & FIRE_ADD))
		{
			gv.pixelMode |= PMODE_GLOW;
		}
		if((gv.pixelMode & FIRE_BLEND) && !(renderMode & FIRE_BLEND))
		{
			gv.pixelMode |= PMODE_BLUR;
		}
		if((gv.pixelMode & PMODE_BLUR) && !(renderMode & PMODE_BLUR))
		{
			gv.pixelMode |= PMODE_FLAT;
		}
		if((gv.pixelMode & PMODE_GLOW) && !(renderMode & PMODE_GLOW))
		{
			gv.pixelMode |= PMODE_BLEND;
		}
		if (renderMode & PMODE_BLOB)
		{
			gv.pixelMode |= PMODE_BLOB;
		}
		gv.pixelMode &= renderMode;

		if (elem.Properties & PROP_HOT_GLOW)
		{
			auto hotglow_low = elem.HighTemperature - 800.0f;
			if (part.temp > hotglow_low)
			{
				auto gradv = 3.1415f / (2 * elem.HighTemperature - hotglow_low);
				auto gaddress = std::min(part.temp, elem.HighTemperature) - hotglow_low;
				gv.colR += int(sin(gradv * gaddress                ) * 226);
				gv.colG += int(sin(gradv * gaddress * 4.55f + 3.14f) *  34);
				gv.colB += int(sin(gradv * gaddress * 2.22f + 3.14f) *  64);
			}
		}

		if (colorMode & (COLOUR_HEAT | COLOUR_LIFE))
		{
			if (gv.pixelMode & (FIREMODE | PMODE_GLOW))
			{
				gv.pixelMode &= ~(FIREMODE | PMODE_GLOW);
				gv.pixelMode |= PMODE_BLUR;
			}
			else if ((gv.pixelMode & (PMODE_BLEND | PMODE_ADD)) == (PMODE_BLEND | PMODE_ADD))
			{
				gv.pixelMode &= ~(PMODE_BLEND | PMODE_ADD);
				gv.pixelMode |= PMODE_FLAT;
			}
			else if (!gv.pixelMode)
			{
				gv.pixelMode |= PMODE_FLAT;
			}
		}
		if (colorMode & COLOUR_GRAD)
		{
			if (gv.pixelMode & (FIREMODE | PMODE_GLOW))
			{
				gv.pixelMode &= ~(FIREMODE | PMODE_GLOW);
				gv.pixelMode |= PMODE_BLUR;
			}
		}

		// * TODO-REDO_UI: These are mutually exclusive; why are they bits?
		if (colorMode & COLOUR_HEAT)
		{
			// * These really shouldn't be macros.
			const float temp_min = MIN_TEMP;
			const float temp_max = MAX_TEMP;
			int caddress = int((part.temp - temp_min) / (temp_max - temp_min)) * 1024;
			if (caddress <    0) caddress =    0;
			if (caddress > 1023) caddress = 1023;
			gv.colA = 255;
			gv.colR = color_data[caddress    ];
			gv.colG = color_data[caddress + 1];
			gv.colB = color_data[caddress + 2];
			gv.firA = gv.colA;
			gv.firR = gv.colR;
			gv.firG = gv.colG;
			gv.firB = gv.colB;

		}
		else if (colorMode & COLOUR_LIFE)
		{
			auto level = int(sinf(0.4f * (part.life >= 5 ? sqrtf(float(part.life)) : float(part.life))) * 100 + 128);
			gv.colA = 255;
			gv.colR = level;
			gv.colG = level;
			gv.colB = level;
		}

		if (!(colorMode & ~COLOUR_GRAD) && decorationsEnabled && decA)
		{
			decA++;
			if (!(gv.pixelMode & NO_DECO))
			{
				gv.colR = (decA * decR + (256 - decA) * gv.colR) >> 8;
				gv.colG = (decA * decG + (256 - decA) * gv.colG) >> 8;
				gv.colB = (decA * decB + (256 - decA) * gv.colB) >> 8;
			}
			if (gv.pixelMode & DECO_FIRE)
			{
				gv.firR = (decA * decR + (256 - decA) * gv.firR) >> 8;
				gv.firG = (decA * decG + (256 - decA) * gv.firG) >> 8;
				gv.firB = (decA * decB + (256 - decA) * gv.firB) >> 8;
			}
		}

		if (colorMode & COLOUR_GRAD)
		{
			auto offset = int(sinf(0.05f * (part.temp - 40)) * 16);
			gv.colR += offset;
			gv.colG += offset;
			gv.colB += offset;
		}

		if (gv.colR > 255) gv.colR = 255;
		if (gv.colR <   0) gv.colR =   0;
		if (gv.colG > 255) gv.colG = 255;
		if (gv.colG <   0) gv.colG =   0;
		if (gv.colB > 255) gv.colB = 255;
		if (gv.colB <   0) gv.colB =   0;
		if (gv.colA > 255) gv.colA = 255;
		if (gv.colA <   0) gv.colA =   0;

		if (gv.firR > 255) gv.firR = 255;
		if (gv.firR <   0) gv.firR =   0;
		if (gv.firG > 255) gv.firG = 255;
		if (gv.firG <   0) gv.firG =   0;
		if (gv.firB > 255) gv.firB = 255;
		if (gv.firB <   0) gv.firB =   0;
		if (gv.firA > 255) gv.firA = 255;
		if (gv.firA <   0) gv.firA =   0;

		auto addPixelCross = [this](int x, int y, int o, uint32_t rgba) {
			AddPixel<true>(x + o, y    , rgba);
			AddPixel<true>(x - o, y    , rgba);
			AddPixel<true>(x    , y + o, rgba);
			AddPixel<true>(x    , y - o, rgba);
		};
		auto blendPixelCross = [this](int x, int y, int o, uint32_t rgba) {
			BlendPixel<true>(x + o, y    , rgba);
			BlendPixel<true>(x - o, y    , rgba);
			BlendPixel<true>(x    , y + o, rgba);
			BlendPixel<true>(x    , y - o, rgba);
		};
		auto blendPixelCrossDiag = [this](int x, int y, int o, uint32_t rgba) {
			BlendPixel<true>(x + o, y - o, rgba);
			BlendPixel<true>(x - o, y + o, rgba);
			BlendPixel<true>(x + o, y + o, rgba);
			BlendPixel<true>(x - o, y - o, rgba);
		};

		if (gv.pixelMode & EFFECT_LINES)
		{
			if (t == PT_SOAP)
			{
				if ((part.ctype & 3) == 3 && part.tmp >= 0 && part.tmp < NPART)
				{
					auto fx = int(parts[part.tmp].x + 0.5f);
					auto fy = int(parts[part.tmp].y + 0.5f);
					DrawLine(nx, ny, fx, fy, PixRGB(gv.colR, gv.colG, gv.colB));
				}
			}
		}
		if (gv.pixelMode & PSPEC_STICKMAN)
		{
			if (t == PT_STKM)
			{
				DrawStickman(nx, ny, t, simulation, simulation.player, PixRGB(gv.colR, gv.colG, gv.colB));
			}
			if (t == PT_STKM2)
			{
				DrawStickman(nx, ny, t, simulation, simulation.player2, PixRGB(gv.colR, gv.colG, gv.colB));
			}
			if (t == PT_FIGH && part.tmp >= 0 && part.tmp < MAX_FIGHTERS)
			{
				DrawStickman(nx, ny, t, simulation, simulation.fighters[part.tmp], PixRGB(gv.colR, gv.colG, gv.colB));
			}
		}
		if (gv.pixelMode & (PMODE_FLAT | PMODE_BLOB))
		{
			SetPixel<false>(nx, ny, PixRGB(gv.colR, gv.colG, gv.colB));
		}
		if (gv.pixelMode & PMODE_BLEND)
		{
			BlendPixel<false>(nx, ny, PixRGBA(gv.colR, gv.colG, gv.colB, gv.colA));
		}
		if (gv.pixelMode & PMODE_ADD)
		{
			AddPixel<false>(nx, ny, PixRGBA(gv.colR, gv.colG, gv.colB, gv.colA));
		}
		if (gv.pixelMode & PMODE_BLOB)
		{
			DrawBlob(nx, ny, PixRGBA(gv.colR, gv.colG, gv.colB, 223));
		}
		if (gv.pixelMode & PMODE_GLOW)
		{
			int a1 = 5 * gv.colA / 256;
			for (int y = -5; y <= 5; ++y)
			{
				int l = (y < 0) ? -y : y;
				l = (l < 2) ? 5 : (7 - l);
				for (int x = -l; x <= l; ++x)
				{
					AddPixel<true>(nx + x, ny + y, PixRGBA(gv.colR, gv.colG, gv.colB, a1));
				}
			}
			int a2 = 180 * gv.colA / 256;
			AddPixel<false>(nx, ny, PixRGBA(gv.colR, gv.colG, gv.colB, a2));
			addPixelCross(nx, ny, 1, PixRGBA(gv.colR, gv.colG, gv.colB, a2 / 2));
		}
		if (gv.pixelMode & PMODE_BLUR)
		{
			for (int y = -3; y <= 3; ++y)
			{
				for (int x = -3; x <= 3; ++x)
				{
					BlendPixel<true>(nx + x, ny + y, PixRGBA(gv.colR, gv.colG, gv.colB, 54 - 4 * (y * y + x * x)));
				}
			}
		}
		if (gv.pixelMode & PMODE_SPARK)
		{
			auto gradv = float(4 * part.life + random_gen() % 20);
			for (auto off = 0; gradv > 0.5f; ++off)
			{
				addPixelCross(nx, ny, 1, PixRGBA(gv.colR, gv.colG, gv.colB, uint8_t(gradv)));
				gradv /= 1.5f;
			}
		}
		if (gv.pixelMode & (PMODE_FLARE | PMODE_LFLARE))
		{
			float step = (gv.pixelMode & PMODE_FLARE) ? 1.2f : 1.01f;
			float gradv = fabs(part.vx) * 17 + fabs(part.vy) * 17 + random_gen() % 20;
			float gradv4 = 4 * gradv;
			if (gradv4 > 255) gradv4 = 255;
			BlendPixel<false>(nx, ny, PixRGBA(gv.colR, gv.colG, gv.colB, int(gradv4)));
			float gradv2 = 2 * gradv;
			if (gradv2 > 255) gradv2 = 255;
			blendPixelCross(nx, ny, 1, PixRGBA(gv.colR, gv.colG, gv.colB, int(gradv2)));
			if (gradv > 255) gradv = 255;
			blendPixelCrossDiag(nx, ny, 1, PixRGBA(gv.colR, gv.colG, gv.colB, int(gradv)));
			for (auto off = 1; gradv > 0.5f; ++off)
			{
				addPixelCross(nx, ny, off, PixRGBA(gv.colR, gv.colG, gv.colB, int(gradv)));
				gradv /= step;
			}
		}
		if (gv.pixelMode & (EFFECT_GRAVIN | EFFECT_GRAVOUT))
		{
			int ignore = (gv.pixelMode & EFFECT_GRAVIN) ? PT_PRTO : PT_PRTI;
			for (int r = 0; r < 32; r += 8)
			{
				int orbd = (part.life >> r) & 0xFF;
				int orbl = (part.ctype >> r) & 0xFF;
				auto ddist = orbd / 16.0f;
				auto drad = orbl / 180.0f * 1.41f * M_PI;
				auto x = int(ddist * cos(drad));
				auto y = int(ddist * sin(drad));
				if (ny + y > 0 && ny + y < YRES && nx + x > 0 && nx + x < XRES && TYP(simulation.pmap[ny + y][nx + x]) != ignore)
				{
					AddPixel<false>(nx + x, ny + y, PixRGBA(gv.colR, gv.colG, gv.colB, 255 - orbd));
				}
			}
		}
		if (gv.firA)
		{
			uint32_t rgb = fire[(ny / CELL) * (XRES / CELL) + (nx / CELL)];
			uint32_t r = PixR(rgb);
			uint32_t g = PixG(rgb);
			uint32_t b = PixB(rgb);
			if (gv.firA && (gv.pixelMode & FIRE_BLEND))
			{
				gv.firA /= 2;
				r = (gv.firA * gv.firR + (255 - gv.firA) * r) >> 8;
				g = (gv.firA * gv.firG + (255 - gv.firA) * g) >> 8;
				b = (gv.firA * gv.firB + (255 - gv.firA) * b) >> 8;
			}
			if (gv.firA && (gv.pixelMode & FIRE_ADD))
			{
				gv.firA /= 8;
				gv.firR = ((gv.firA * gv.firR) >> 8) + r;
				gv.firG = ((gv.firA * gv.firG) >> 8) + g;
				gv.firB = ((gv.firA * gv.firB) >> 8) + b;
				if (gv.firR > 255) gv.firR = 255;
				if (gv.firG > 255) gv.firG = 255;
				if (gv.firB > 255) gv.firB = 255;
				r = gv.firR;
				g = gv.firG;
				b = gv.firB;
			}
			if (gv.firA && (gv.pixelMode & FIRE_SPARK))
			{
				gv.firA /= 4;
				r = (gv.firA * gv.firR + (255 - gv.firA) * r) >> 8;
				g = (gv.firA * gv.firG + (255 - gv.firA) * g) >> 8;
				b = (gv.firA * gv.firB + (255 - gv.firA) * b) >> 8;
			}
			fire[(ny / CELL) * (XRES / CELL) + (nx / CELL)] = PixRGB(r, g, b);
		}
	}
}

void SimulationRenderer::UpdatePersistent()
{
	if (displayMode & DISPLAY_PERS)
	{
		for (int i = 0; i < XRES * YRES; ++i)
		{
			auto rgb = mainBuffer[i];
			auto r = PixR(rgb);
			auto g = PixG(rgb);
			auto b = PixB(rgb);
			if (r > 0) --r;
			if (g > 0) --g;
			if (b > 0) --b;
			persBuffer[i] = PixRGB(r, g, b);
		}
	}
}

void SimulationRenderer::RenderFire()
{
	if (renderMode & FIREMODE)
	{
		for (int y = 0; y < YRES / CELL; ++y)
		{
			for (int x = 0; x < XRES / CELL; ++x)
			{
				uint32_t rgb = fire[y * (XRES / CELL) + x];
				uint32_t r = PixR(rgb);
				uint32_t g = PixG(rgb);
				uint32_t b = PixB(rgb);
				if (r || g || b)
				{
					for (int j = -CELL; j < 2 * CELL; ++j)
					{
						for (int i = -CELL; i < 2 * CELL; ++i)
						{
							AddPixel<true>(x * CELL + i, y * CELL + j, PixRGBA(r, g, b, firA[j + CELL][i + CELL]));
						}
					}
				}
				r *= 8;
				g *= 8;
				b *= 8;
				for (int j = -1; j < 2; ++j)
				{
					for (int i = -1; i < 2; ++i)
					{
						if ((i || j) && x + i >= 0 && y + j >= 0 && x + i < XRES / CELL && y + j < YRES / CELL)
						{
							uint32_t rgb2 = fire[(y + j) * (XRES / CELL) + (x + i)];
							r += PixR(rgb2);
							g += PixG(rgb2);
							b += PixB(rgb2);
						}
					}
				}
				r /= 16;
				g /= 16;
				b /= 16;
				fire[y * (XRES / CELL) + x] = PixRGB(r > 4 ? r - 4 : 0, g > 4 ? g - 4 : 0, b > 4 ? b - 4 : 0);
			}
		}
	}
}

void SimulationRenderer::RenderEMP(const Simulation &simulation)
{
	if (renderMode & EFFECT)
	{
		int emp_decor = simulation.emp_decor;
		if (emp_decor > 40) emp_decor = 40;
		if (emp_decor <  0) emp_decor =  0;
		if (emp_decor > 0)
		{
			auto r = int(emp_decor * 2.5);
			auto g = int(emp_decor * 1.5 + 100);
			auto a = int(emp_decor / 110.f * 255);
			for (int y = 0; y < YRES; ++y)
			{
				for (int x = 0; x < XRES; ++x)
				{
					BlendPixel<false>(x, y, PixRGBA(r, g, 255, a));
				}
			}
		}
	}
}

void SimulationRenderer::RenderGravityZones(const Simulation &simulation)
{
	if (gravityZonesEnabled)
	{
		for (int y = 0; y < YRES / CELL; ++y)
		{
			for (int x = 0; x < XRES / CELL; ++x)
			{
				if (simulation.grav->gravmask[y * (XRES / CELL) + x])
				{
					for (int j = 0; j < CELL; ++j)
					{
						for (int i = 0; i < CELL; ++i)
						{
							if (i == j)
							{
								BlendPixel<false>(x * CELL + i, y * CELL + j, PixRGBA(255, 200, 0, 120));
							}
							else
							{
								BlendPixel<false>(x * CELL + i, y * CELL + j, PixRGBA(32, 32, 32, 120));
							}
						}
					}
				}
			}
		}
	}
}

static const unsigned char *fontData(String::value_type ch)
{
	auto &fd = gui::GetFontData();
	int pos = 0;
	for (auto *range = fd.ranges; (*range)[1]; ++range)
	{
		if ((*range)[0] <= ch && (*range)[1] >= ch)
		{
			return fd.bits + fd.ptrs[pos + (ch - (*range)[0])];
		}
		pos += (*range)[1] - (*range)[0] + 1;
	}
	if (ch == 0xFFFD)
	{
		return fd.bits;
	}
	else
	{
		return fontData(0xFFFD);
	}
}

void SimulationRenderer::RenderSigns(const Simulation &simulation)
{
	for (auto &s : simulation.signs)
	{
		if (s.text.length())
		{
			auto info = s.GetInfo(&simulation);
			info.w += 1;
			for (auto y = 0; y < info.h; ++y)
			{
				for (auto x = 0; x < info.w; ++x)
				{
					if (x && y && x < info.w - 1 && y < info.h - 1)
					{
						BlendPixel<true>(info.x + x, info.y + y, PixRGBA(0, 0, 0, 255));
					}
					else
					{
						BlendPixel<true>(info.x + x, info.y + y, PixRGBA(192, 192, 192, 255));
					}
				}
			}
			info.x += 3;
			info.y += 1;
			for (auto it = info.text.begin(); it != info.text.end(); ++it)
			{
				auto *data = fontData(*it);
				int width = *(data++);
				int bits = 0;
				char buffer;
				for (auto y = 0; y < FONT_H; ++y)
				{
					for (auto x = 0; x < width; ++x)
					{
						if (!bits)
						{
							bits = 8;
							buffer = *(data++);
						}
						BlendPixel<true>(info.x + x, info.y + y + 1, PixRGBA(info.r, info.g, info.b, (buffer & 3) * 85));
						bits -= 2;
						buffer >>= 2;
					}
				}
				info.x += width;
			}
			if (s.ju != sign::None)
			{
				int x = s.x;
				int y = s.y;
				int dx = 1 - s.ju;
				int dy = (s.y > 18) ? -1 : 1;
				for (int j = 0; j < 4; j++)
				{
					BlendPixel<true>(x, y, PixRGBA(192, 192, 192, 255));
					x += dx;
					y += dy;
				}
			}
		}
	}
}

void SimulationRenderer::RenderGravityLensing(const Simulation &simulation)
{
	if (displayMode & DISPLAY_WARP)
	{
		std::swap(mainBuffer, warpBuffer);
		for (int ny = 0; ny < YRES; ++ny)
		{
			for (int nx = 0; nx < XRES; ++nx)
			{
				int co = (ny / CELL) * (XRES / CELL) + (nx / CELL);
				int rx = int(nx - simulation.gravx[co] *  0.75f + 0.5f);
				int ry = int(ny - simulation.gravy[co] *  0.75f + 0.5f);
				int gx = int(nx - simulation.gravx[co] * 0.875f + 0.5f);
				int gy = int(ny - simulation.gravy[co] * 0.875f + 0.5f);
				int bx = int(nx - simulation.gravx[co]          + 0.5f);
				int by = int(ny - simulation.gravy[co]          + 0.5f);
				if (rx >= 0 && rx < XRES && ry >= 0 && ry < YRES && gx >= 0 && gx < XRES && gy >= 0 && gy < YRES && bx >= 0 && bx < XRES && by >= 0 && by < YRES)
				{
					int rgb = GetPixel<false>(nx, ny);
					int r = PixR(warpBuffer[ry * XRES + rx]) + PixR(rgb);
					int g = PixG(warpBuffer[gy * XRES + gx]) + PixG(rgb);
					int b = PixB(warpBuffer[by * XRES + bx]) + PixB(rgb);
					if (r > 255) r = 255;
					if (g > 255) g = 255;
					if (b > 255) b = 255;
					SetPixel<false>(nx, ny, PixRGB(r, g, b));
				}
			}
		}
	}
}

void SimulationRenderer::Render(const Simulation &simulation)
{
	// * TODO-REDO_UI: wifi overlay
	// * TODO-REDO_UI: find overlay
	// * TODO-REDO_UI: stickman life overlay
	RenderPersistent();
	RenderAir(simulation);
	RenderGravity(simulation);
	RenderWalls(simulation);
	RenderGrid();
	RenderParticles(simulation);
	UpdatePersistent();
	RenderFire();
	RenderEMP(simulation);
	RenderGravityZones(simulation);
	RenderSigns(simulation);
	RenderGravityLensing(simulation);
}

void SimulationRenderer::ClearAccumulation()
{
	std::fill(fire.begin(), fire.end(), 0U);
	std::fill(persBuffer.begin(), persBuffer.end(), 0U);
}

struct GradientStop
{
	int color;
	float point;

	bool operator <(const GradientStop &other) const
	{
		return point < other.point;
	}
};

static void gradient(std::vector<uint32_t> &table, std::vector<GradientStop> stops, int resolution)
{
	table.resize(resolution, 0);
	if (stops.size() >= 2)
	{
		std::sort(stops.begin(), stops.end());
		auto stop = -1;
		for (auto i = 0; i < resolution; ++i)
		{
			auto point = i / (float)resolution;
			while (stop < (int)stops.size() - 1 && stops[stop + 1].point <= point)
			{
				++stop;
			}
			if (stop < 0 || stop >= (int)stops.size() - 1)
			{
				continue;
			}
			auto &left = stops[stop];
			auto &right = stops[stop + 1];
			auto f = (point - left.point) / (right.point - left.point);
			table[i] = PixRGB(
				int(PixR(left.color) * (1.0f - f) + PixR(right.color) * f),
				int(PixG(left.color) * (1.0f - f) + PixG(right.color) * f),
				int(PixB(left.color) * (1.0f - f) + PixB(right.color) * f)
			);
		}
	}
}

void SimulationRenderer::FlushGraphicsCache(int begin, int end)
{
	std::fill(&graphicsCache[begin], &graphicsCache[end], SimulationGraphicsValues());
}

void SimulationRenderer::InitGraphicsTables()
{
	gradient(flameTable, {
		{ 0xAF9F0F, 1.0f },
		{ 0xDFBF6F, 0.9f },
		{ 0x60300F, 0.5f },
		{ 0x000000, 0.0f },
	}, 200);
	gradient(plasmaTable, {
		{ 0xAFFFFF,  1.0f },
		{ 0xAFFFFF,  0.9f },
		{ 0x301060,  0.5f },
		{ 0x301040, 0.25f },
		{ 0x000000,  0.0f },
	}, 200);

	float temp[CELL * 3][CELL * 3] = {};
	float multiplier = 255.0f;
	for (int x = 0; x < CELL; ++x)
	{
		for (int y = 0; y < CELL; ++y)
		{
			for (int i = -CELL; i < CELL; ++i)
			{
				for (int j = -CELL; j < CELL; ++j)
				{
					temp[y + CELL + j][x + CELL + i] += expf(-0.1f * (i * i + j * j));
				}
			}
		}
	}
	for (int x = 0; x < CELL * 3; ++x)
	{
		for (int y = 0; y < CELL * 3; ++y)
		{
			firA[y][x] = (int)(multiplier * temp[y][x] / (CELL * CELL));
		}
	}
}

void SimulationRenderer::ColorMode(int newColorMode)
{
	colorMode = newColorMode;
}

void SimulationRenderer::DisplayMode(int newDisplayMode)
{
	auto oldDisplayMode = displayMode;
	displayMode = newDisplayMode;
	if (!(displayMode & DISPLAY_PERS) && (oldDisplayMode & DISPLAY_PERS))
	{
		ClearAccumulation();
	}
}

void SimulationRenderer::RenderMode(int newRenderMode)
{
	auto oldRenderMode = renderMode;
	renderMode = newRenderMode;
	if (!(renderMode & FIREMODE) && (oldRenderMode & FIREMODE))
	{
		ClearAccumulation();
	}
}
