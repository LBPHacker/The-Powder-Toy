#pragma once
#include <cstdint>
#include <common/Vec2.h>

constexpr int MENUSIZE = 40;
constexpr int BARSIZE  = 17;

constexpr float M_GRAV = 6.67300e-1f;

#define SIM_PARAMS(X) \
	X(int      , CELL) \
	X(int      , CELL3) \
	X(Vec2<int>, CELLS) \
	X(Vec2<int>, RES) \
	X(int      , XCELLS) \
	X(int      , YCELLS) \
	X(int      , NCELL) \
	X(int      , XRES) \
	X(int      , YRES) \
	X(int      , NPART) \
	X(int      , XCNTR) \
	X(int      , YCNTR) \
	X(Vec2<int>, WINDOW) \
	X(int      , WINDOWW) \
	X(int      , WINDOWH) \
	// last line of the macro

#define DECL_GETTER(t, n) t n ## _Getter();
SIM_PARAMS(DECL_GETTER)
#undef DECL_GETTER

#define GEN_WRAPPER(t, n) \
	struct n ## _Wrapper \
	{ \
		operator t() const \
		{ \
			return n ## _Getter(); \
		} \
	};
SIM_PARAMS(GEN_WRAPPER)
#undef GEN_WRAPPER

#define CELL    (   CELL_Wrapper{})
#define CELL3   (  CELL3_Wrapper{})
#define CELLS   (  CELLS_Wrapper{})
#define RES     (    RES_Wrapper{})
#define XCELLS  ( XCELLS_Wrapper{})
#define YCELLS  ( YCELLS_Wrapper{})
#define NCELL   (  NCELL_Wrapper{})
#define XRES    (   XRES_Wrapper{})
#define YRES    (   YRES_Wrapper{})
#define NPART   (  NPART_Wrapper{})
#define XCNTR   (  XCNTR_Wrapper{})
#define YCNTR   (  YCNTR_Wrapper{})
#define WINDOW  ( WINDOW_Wrapper{})
#define WINDOWW (WINDOWW_Wrapper{})
#define WINDOWH (WINDOWH_Wrapper{})

inline int operator /(int thing, CELL_Wrapper)
{
	return thing / CELL_Getter();
}

constexpr int MAXSIGNS = 16;

constexpr int   ISTP         = 2;
constexpr float CFDS         = 1.f;
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
