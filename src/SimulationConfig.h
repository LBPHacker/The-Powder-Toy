#pragma once
#include <cstdint>
#include <common/Vec2.h>

constexpr int MENUSIZE = 40;
constexpr int BARSIZE  = 17;

constexpr float M_GRAV = 6.67300e-1f;

//CELL, the size of the pressure, gravity, and wall maps. Larger than 1 to prevent extreme lag
extern int CELL;
extern Vec2<int> CELLS;
extern Vec2<int> RES;

extern int XCELLS;
extern int YCELLS;
extern int NCELL;
extern int XRES;
extern int YRES;
extern int NPART;

extern int XCNTR;
extern int YCNTR;

extern Vec2<int> WINDOW;

extern int WINDOWW;
extern int WINDOWH;

constexpr int MAXSIGNS = 16;

extern int   ISTP;
extern float CFDS;
constexpr float MAX_VELOCITY = 1e4f;

//Air constants
constexpr float AIR_TSTEPP = 0.3f;
constexpr float AIR_TSTEPV = 0.4f;
constexpr float AIR_VADV   = 0.3f;
constexpr float AIR_VLOSS  = 0.999f;
constexpr float AIR_PLOSS  = 0.9999f;

constexpr int NGOL = 24;

enum DefaultBrushes
{
	BRUSH_CIRCLE,
	BRUSH_SQUARE,
	BRUSH_TRIANGLE,
	NUM_DEFAULTBRUSHES,
};

//Photon constants
constexpr int SURF_RANGE     = 10;
constexpr int NORMAL_MIN_EST =  3;
constexpr int NORMAL_INTERP  = 20;
constexpr int NORMAL_FRAC    = 16;

constexpr auto REFRACT = UINT32_C(0x80000000);

/* heavy flint glass, for awesome refraction/dispersion
   this way you can make roof prisms easily */
constexpr float GLASS_IOR  = 1.9f;
constexpr float GLASS_DISP = 0.07f;

constexpr float R_TEMP = 22;

constexpr bool LATENTHEAT = false;
