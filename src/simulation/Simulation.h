#pragma once
#include "Particle.h"
#include "Stickman.h"
#include "WallType.h"
#include "Sign.h"
#include "ElementDefs.h"
#include "BuiltinGOL.h"
#include "MenuSection.h"
#include "AccessProperty.h"
#include "CoordStack.h"
#include "common/tpt-rand.h"
#include "common/ContainerOf.h"
#include "gravity/Gravity.h"
#include "graphics/RendererFrame.h"
#include "Element.h"
#include "SimulationConfig.h"
#include "SimulationSettings.h"
#include "common/ThreadIndex.h"
#include "common/ThreadPool.h"
#include "Misc.h"
#include <cstring>
#include <cstddef>
#include <vector>
#include <array>
#include <memory>
#include <optional>

constexpr int TILE_ROUNDS = 2;

constexpr int TILE = 16; // in cells
constexpr Vec2<int> TILES = { ceilDiv(CELLS.X, TILE).first, ceilDiv(CELLS.Y, TILE).first };

template<class Item>
using TilePlane = PlaneAdapter<std::array<Item, (TILES.X + 1) * (TILES.Y + 1)>, TILES.X + 1, TILES.Y + 1>;

constexpr int CHANNELS = int(MAX_TEMP - 73) / 100 + 2;

class Snapshot;
class Brush;
struct SimulationSample;
struct matrix2d;
struct vector2d;

class Simulation;
class Renderer;
class Air;
class GameSave;

struct alignas(64) ThreadContext
{
	RNG rng;
	int pfree;
	int freeListLength;
	std::array<int, PT_NUM> elementCount;
	int NUM_PARTS;
	int debug_mostRecentlyUpdated;
	std::vector<Vec2<int>> emapActivation;
};

class Parts
{
public:
	int pfree;
	std::mutex pfreeMx;
	int pfreeMxLockedTimes = 0;
	int active;

	std::array<Particle, NPART> data;
	// initialized in clear_sim

	operator const Particle *() const
	{
		return data.data();
	}

	operator Particle *()
	{
		return data.data();
	}

	Parts()
	{
		Reset();
	}

	Parts(const Parts &other) = delete;

	Parts &operator =(const Parts &other)
	{
		std::copy(other.data.begin(), other.data.begin() + other.active, data.begin());
		active = other.active;
		pfree = other.pfree;
		return *this;
	}

	void Reset();
	void Free(int i);
	int Alloc();
	void Flatten();

	bool MaxPartsReached() const
	{
		return false;
	}
};

struct alignas(64) RenderableSimulation
{
	GravityInput gravIn;
	GravityOutput gravOut; // invariant: when grav is empty, this is in its default-constructed state
	bool gravForceRecalc = true;
	std::vector<sign> signs;

	int currentTick = 0;
	int emp_decor = 0;

	playerst player;
	playerst player2;
	playerst fighters[MAX_FIGHTERS]; //Defined in Stickman.h

	alignas(64) float vx[YCELLS][XCELLSPERF];
	alignas(64) float vy[YCELLS][XCELLSPERF];
	alignas(64) float pv[YCELLS][XCELLSPERF];
	alignas(64) float hv[YCELLS][XCELLSPERF];

	alignas(64) unsigned char bmap[YCELLS][XCELLSPERF];
	alignas(64) unsigned char emap[YCELLS][XCELLSPERF];

	Parts parts;
	alignas(64) int pmap[YRES][XRESPERF];
	alignas(64) int photons[YRES][XRESPERF];

	int aheat_enable = 0;

	bool useLuaCallbacks = false;
};

struct RNGMultiplexer
{
	RNG global;

	RNG &Instance();
	const RNG &Instance() const;

	unsigned int operator()()
	{
		return Instance()();
	}

	unsigned int gen()
	{
		return Instance().gen();
	}

	int between(int lower, int upper)
	{
		return Instance().between(lower, upper);
	}

	bool chance(int numerator, unsigned int denominator)
	{
		return Instance().chance(numerator, denominator);
	}

	float uniform01()
	{
		return Instance().uniform01();
	}

	void seed(unsigned int sd)
	{
		Instance().seed(sd);
	}

	void state(RNG::State ns)
	{
		Instance().state(ns);
	}

	RNG::State state() const
	{
		return Instance().state();
	}
};

class alignas(64) Simulation : public RenderableSimulation
{
public:
	alignas(64) GravityPtr grav;
	alignas(64) std::unique_ptr<Air> air;

	alignas(64) bool useThreadContext = false;
	alignas(64) RNGMultiplexer rng;

	int replaceModeSelected = 0;
	int replaceModeFlags = 0;
	int debug_nextToUpdate = 0;
	int debug_mostRecentlyUpdated = -1; // -1 when between full update loops
	std::array<int, PT_NUM> elementCount;
	int ISWIRE = 0;
	bool force_stacking_check = false;
	int emp_trigger_count = 0;
	bool etrd_count_valid = false;
	int etrd_life0_count = 0;
	int lightningRecreate = 0;
	bool gravWallChanged = false;

	Particle portalp[CHANNELS][8][80];
	int wireless[CHANNELS][2];

	int CGOL = 0;
	int GSPEED = 1;
	unsigned int gol[YRES][XRES][5];

	float fvx[YCELLS][XCELLS];
	float fvy[YCELLS][XCELLS];
	int Element_LOLZ_lolz[XRES/9][YRES/9];
	int Element_LOVE_love[XRES/9][YRES/9];
	int Element_PSTN_tempParts[std::max(XRES, YRES)];
	int Element_PPIP_ppip_changed;

	alignas(64) unsigned int pmap_count[YRES][XRESPERF];

	int edgeMode = EDGE_VOID;
	int gravityMode = GRAV_VERTICAL;
	float customGravityX = 0;
	float customGravityY = 0;
	int legacy_enable = 0;
	int water_equal_test = 0;
	int pretty_powder = 0;
	int sandcolour_frame = 0;
	int deco_space = DECOSPACE_SRGB;

	// initialized in clear_sim
	bool elementRecount;
	unsigned char fighcount; //Contains the number of fighters
	uint64_t frameCount;
	bool ensureDeterminism;

	// initialized very late >_>
	int NUM_PARTS;
	int sandcolour;
	int sandcolour_interface;

	void Load(const GameSave *save, bool includePressure, Vec2<int> blockP); // block coordinates
	std::unique_ptr<GameSave> Save(bool includePressure, Rect<int> partR); // particle coordinates
	void SaveSimOptions(GameSave &gameSave);
	SimulationSample GetSample(int x, int y);

	std::unique_ptr<Snapshot> CreateSnapshot() const;
	void Restore(const Snapshot &snap);

	int is_blocking(int t, int x, int y) const;
	int is_boundary(int pt, int x, int y) const;
	int find_next_boundary(int pt, int *x, int *y, int dm, int *em, bool reverse) const;
	void photoelectric_effect(int nx, int ny);
	int do_move(int i, int x, int y, float nxf, float nyf);
	bool move(int i, int x, int y, float nxf, float nyf);
	int try_move(int i, int x, int y, int nx, int ny);
	int eval_move(int pt, int nx, int ny, unsigned *rr) const;

	struct PlanMoveResult
	{
		int fin_x, fin_y, clear_x, clear_y;
		float fin_xf, fin_yf, clear_xf, clear_yf;
		float vx, vy;
	};
	template<bool UpdateEmap, class Sim>
	static PlanMoveResult PlanMove(Sim &sim, int i, int x, int y);

	bool IsWallBlocking(int x, int y, int type) const;
	void create_cherenkov_photon(int pp);
	void create_gain_photon(int pp);
	void kill_part(int i);
	bool FloodFillPmapCheck(int x, int y, int type) const;
	int flood_prop(int x, int y, const AccessProperty &changeProperty);
	bool flood_water(int x, int y, int i);
	int FloodINST(int x, int y);
	void detach(int i);
	bool part_change_type(int i, int x, int y, int t);
	//int InCurrentBrush(int i, int j, int rx, int ry);
	//int get_brush_flags();
	int create_part(int p, int x, int y, int t, int v = -1);
	void delete_part(int x, int y);
	void get_sign_pos(int i, int *x0, int *y0, int *w, int *h);
	int is_wire(int x, int y);
	int is_wire_off(int x, int y);
	void set_emap(int x, int y);
	int parts_avg(int ci, int ni, int t);
	void UpdateParticles(int start, int end); // Dispatches an update to the range [start, end).
	std::vector<std::pair<ByteString, double>> updatePhaseTimes;
	void SimulateGoL();
	void RecalcFreeParticles(bool do_life_dec);
	void CheckStacking();
	void BeforeSim(bool willUpdate);
	void AfterSim();
	void clear_area(int area_x, int area_y, int area_w, int area_h);

	void SetEdgeMode(int newEdgeMode);
	void SetDecoSpace(int newDecoSpace);

	//Drawing Deco
	void ApplyDecoration(int x, int y, int colR, int colG, int colB, int colA, int mode);
	void ApplyDecorationPoint(int x, int y, int colR, int colG, int colB, int colA, int mode, Brush const &cBrush);
	void ApplyDecorationLine(int x1, int y1, int x2, int y2, int colR, int colG, int colB, int colA, int mode, Brush const &cBrush);
	void ApplyDecorationBox(int x1, int y1, int x2, int y2, int colR, int colG, int colB, int colA, int mode);
	bool ColorCompare(const RendererFrame &frame, int x, int y, int replaceR, int replaceG, int replaceB);
	void ApplyDecorationFill(const RendererFrame &frame, int x, int y, int colR, int colG, int colB, int colA, int replaceR, int replaceG, int replaceB);

	//Drawing Walls
	int CreateWalls(int x, int y, int rx, int ry, int wall, Brush const *cBrush);
	void CreateWallLine(int x1, int y1, int x2, int y2, int rx, int ry, int wall, Brush const *cBrush);
	void CreateWallBox(int x1, int y1, int x2, int y2, int wall);
	int FloodWalls(int x, int y, int wall, int bm);

	//Drawing Particles
	int CreateParts(int p, int positionX, int positionY, int c, Brush const &cBrush, int flags);
	int CreateParts(int p, int x, int y, int rx, int ry, int c, int flags);
	int CreatePartFlags(int p, int x, int y, int c, int flags);
	void CreateLine(int x1, int y1, int x2, int y2, int c, Brush const &cBrush, int flags);
	void CreateLine(int x1, int y1, int x2, int y2, int c);
	void CreateBox(int p, int x1, int y1, int x2, int y2, int c, int flags);
	int FloodParts(int x, int y, int c, int cm, int flags);

	void GetGravityField(int x, int y, float particleGrav, float newtonGrav, float & pGravX, float & pGravY) const;

	int get_wavelength_bin(int *wm);
	struct GetNormalResult
	{
		bool success;
		float nx, ny;
		int lx, ly, rx, ry;
	};
	GetNormalResult get_normal(int pt, int x, int y, float dx, float dy) const;
	template<bool PhotoelectricEffect, class Sim>
	static GetNormalResult get_normal_interp(Sim &sim, int pt, float x0, float y0, float dx, float dy);
	void clear_sim();
	Simulation();
	~Simulation();

	void EnableNewtonianGravity(bool enable);

	void SetTileThreadCount(int newThreadCount);
	int GetTileThreadCount() const
	{
		return tileThreadCount;
	}

	std::vector<ThreadContext> threadContexts;

private:
	CoordStack& getCoordStackSingleton();

	void ResetNewtonianGravity(GravityInput newGravIn, GravityOutput newGravOut);
	void DispatchNewtonianGravity();
	void UpdateGravityMask();

	struct Neighbourhood
	{
		std::array<int, 8> surround;
		int surround_space = 0;
		int nt = 0; //if nt is greater than 1 after this, then there is a particle around the current particle, that is NOT the current particle's type, for water movement.
		float pGravX = 0;
		float pGravY = 0;
	};
	void MovementPhase(int i, Neighbourhood neighbourhood);
	Neighbourhood GetNeighbourhood(int i) const;
	bool TransitionPhase(int i, const Neighbourhood &neighbourhood);

	struct alignas(64) TileInfo
	{
		std::vector<int, AlignedAllocator<int>> parts;
		std::vector<int, AlignedAllocator<int>> partsDeferredMovement;
		std::vector<int, AlignedAllocator<int>> partsDeferred;
	};
	struct alignas(64) TileRoundInfo
	{
		alignas(64) TilePlane<TileInfo> tiles;
		Vec2<int> offset{ 0, 0 };
		std::vector<int> partsDeferred;
	};
	alignas(64) std::array<TileRoundInfo, TILE_ROUNDS> tileRounds;
	void UpdateOne(int i, int deferTiledMovement);
	bool EligibleForTiledUpdate(int i, int tileRoundIndex) const;

	int tileThreadCount = 1;
	ThreadPool tileThreads;

	template<class Func>
	void TilePartilces(Func func, int tileRoundIndex);
};

inline RNG &RNGMultiplexer::Instance()
{
	auto *sim = ContainerOf<&Simulation::rng>(this);
	return sim->useThreadContext ? sim->threadContexts[ThreadIndex()].rng : global;
}

inline const RNG &RNGMultiplexer::Instance() const
{
	auto *sim = ContainerOf<&Simulation::rng>(this);
	return sim->useThreadContext ? sim->threadContexts[ThreadIndex()].rng : global;
}
