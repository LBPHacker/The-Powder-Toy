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
#include "gravity/Gravity.h"
#include "graphics/RendererFrame.h"
#include "SimulationConfig.h"
#include "SimulationSettings.h"
#include "SimImpls.h"
#include <cstring>
#include <cstddef>
#include <vector>
#include <array>
#include <memory>
#include <optional>

constexpr int CHANNELS = int(MAX_TEMP - 73) / 100 + 2;

class FrameTime;
class Snapshot;
class Brush;
struct SimulationSample;
struct matrix2d;
struct vector2d;

class Simulation;
class Renderer;
class Air;
class GameSave;

class Parts
{
public:
	int pfree;

	std::array<Particle, NPART> data;
	// initialized in clear_sim
	int active;

	operator const Particle *() const
	{
		return data.data();
	}

	operator Particle *()
	{
		return data.data();
	}

	Parts();

	Parts(const Parts &other) = delete;

	Parts &operator =(const Parts &other)
	{
		std::copy(other.data.begin(), other.data.begin() + other.active, data.begin());
		active = other.active;
		pfree = other.pfree;
		return *this;
	}

	void Reset();
};

struct RenderableSimulation
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

	float vx[YCELLS][XCELLS];
	float vy[YCELLS][XCELLS];
	float pv[YCELLS][XCELLS];
	float hv[YCELLS][XCELLS];

	unsigned char bmap[YCELLS][XCELLS];
	unsigned char emap[YCELLS][XCELLS];

	Parts parts;
	int pmap[YRES][XRES];
	int photons[YRES][XRES];

	int aheat_enable = 0;

	bool useLuaCallbacks = false;
};

struct CopiableSimulation : public RenderableSimulation
{
	RNG rng;

	int replaceModeSelected = 0;
	int replaceModeFlags = 0;
	int debug_nextToUpdate = 0;
	int debug_mostRecentlyUpdated = -1; // -1 when between full update loops
	int elementCount[PT_NUM];
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

	unsigned int pmap_count[YRES][XRES];

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

	FrameTime *frameTime = nullptr;
};

class Simulation : public CopiableSimulation
{
public:
	GravityPtr grav;
	std::unique_ptr<Air> air;

	Simulation(const Simulation &other) = delete;

	void CopyFrom(const Simulation &other);

	void Load(const GameSave *save, bool includePressure, Vec2<int> blockP); // block coordinates
	std::unique_ptr<GameSave> Save(bool includePressure, Rect<int> partR); // particle coordinates
	void SaveSimOptions(GameSave &gameSave);
	SimulationSample GetSample(int x, int y);

	std::unique_ptr<Snapshot> CreateSnapshot() const;
	void Restore(const Snapshot &snap);

	virtual bool move_outer(int i, int x, int y, float nxf, float nyf) = 0;
	virtual int eval_move_outer(int pt, int nx, int ny, unsigned *rr) const = 0;

	struct PlanMoveResult
	{
		int fin_x, fin_y, clear_x, clear_y;
		float fin_xf, fin_yf, clear_xf, clear_yf;
		float vx, vy;
	};
	virtual PlanMoveResult PlanMoveOuter(int i, int x, int y) const = 0;

	bool IsWallBlocking(int x, int y, int type) const;
	virtual void kill_part_outer(int i) = 0;
	bool FloodFillPmapCheck(int x, int y, int type) const;
	int flood_prop(int x, int y, const AccessProperty &changeProperty);
	virtual bool part_change_type_outer(int i, int x, int y, int t) = 0;
	virtual int create_part_outer(int p, int x, int y, int t, int v = -1) = 0;
	void delete_part(int x, int y);
	virtual void set_emap_outer(int x, int y) = 0;
	int parts_avg(int ci, int ni, int t);
	virtual void UpdateParticles(int start, int end) = 0; // Dispatches an update to the range [start, end).
	virtual void RecalcFreeParticlesOuter(bool do_life_dec) = 0;
	virtual void BeforeSim(bool willUpdate) = 0;
	virtual void AfterSim() = 0;
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
	virtual void CreateLineOuter(int x1, int y1, int x2, int y2, int c) = 0;
	void CreateBox(int p, int x1, int y1, int x2, int y2, int c, int flags);
	int FloodParts(int x, int y, int c, int cm, int flags);

	void GetGravityField(int x, int y, float particleGrav, float newtonGrav, float & pGravX, float & pGravY) const;

	struct GetNormalResult
	{
		bool success;
		float nx, ny;
		int lx, ly, rx, ry;
	};
	virtual GetNormalResult get_normal_interp_outer(int pt, float x0, float y0, float dx, float dy) const = 0;

	void clear_sim();
	Simulation();
	virtual ~Simulation();

	void EnableNewtonianGravity(bool enable);

	static std::unique_ptr<Simulation> LegacyFactory();

protected:
	CoordStack& getCoordStackSingleton();

	void ResetNewtonianGravity(GravityInput newGravIn, GravityOutput newGravOut);
	void DispatchNewtonianGravity();
	void UpdateGravityMask();
};

template<class VariantParam>
class SimVariant : public virtual Simulation
{
public:
	void set_emap(int x, int y);
	void kill_part(int i);
	bool part_change_type(int i, int x, int y, int t);
	int create_part(int p, int x, int y, int t, int v = -1);
	int FloodINST(int x, int y);
	void CreateLine(int x1, int y1, int x2, int y2, int c);
	bool move(int i, int x, int y, float nxf, float nyf);
	int createPartTempVel(int i, int x, int y, int t);
	int eval_move(int pt, int nx, int ny, unsigned *rr) const;

	inline bool MaxPartsReached() const;

	using Variant = VariantParam;
};

template<>
inline bool SimVariant<LegacyVariant>::MaxPartsReached() const
{
	return parts.pfree == -1;
}
