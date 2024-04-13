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
#include "common/Plane.h"
#include "Element.h"
#include "SimulationConfig.h"
#include "SimulationSettings.h"
#include <cstring>
#include <cstddef>
#include <vector>
#include <array>
#include <memory>
#include <optional>

constexpr int CHANNELS = 101;

class Snapshot;
class Brush;
struct SimulationSample;
struct matrix2d;
struct vector2d;

class Simulation;
class Renderer;
class Air;
class GameSave;

struct Parts
{
	std::vector<Particle> data;
	// initialized in clear_sim
	int lastActiveIndex;

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

	Parts(const Parts &other) = default;

	Parts &operator =(const Parts &other)
	{
		data.resize(other.data.size());
		std::copy(other.data.begin(), other.data.begin() + other.lastActiveIndex + 1, data.begin());
		lastActiveIndex = other.lastActiveIndex;
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
	std::array<playerst, MAX_FIGHTERS> fighters; //Defined in Stickman.h

	PlaneAdapter<std::vector<float>> vx;
	PlaneAdapter<std::vector<float>> vy;
	PlaneAdapter<std::vector<float>> pv;
	PlaneAdapter<std::vector<float>> hv;

	PlaneAdapter<std::vector<unsigned char>> bmap;
	PlaneAdapter<std::vector<unsigned char>> emap;

	Parts parts;
	PlaneAdapter<std::vector<int>> pmap;
	PlaneAdapter<std::vector<int>> photons;

	int aheat_enable = 0;

	bool useLuaCallbacks = false;
};

class Simulation : public RenderableSimulation
{
public:
	RNG rng;

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

	std::array<std::array<std::array<Particle, 80>, 8>, CHANNELS> portalp;
	std::array<std::array<int, 2>, CHANNELS> wireless;

	int CGOL = 0;
	int GSPEED = 1;
	PlaneAdapter<std::vector<std::array<unsigned int, 5>>> gol;

	PlaneAdapter<std::vector<float>> fvx;
	PlaneAdapter<std::vector<float>> fvy;

	PlaneAdapter<std::vector<unsigned int>> pmap_count;
	//Lolz
	PlaneAdapter<std::vector<int>> Element_LOLZ_lolz;
	PlaneAdapter<std::vector<int>> Element_LOVE_love;
	//PSTN
	std::vector<int> pstnTempParts;

	int edgeMode = EDGE_VOID;
	int gravityMode = GRAV_VERTICAL;
	float customGravityX = 0;
	float customGravityY = 0;
	int legacy_enable = 0;
	int water_equal_test = 0;
	int sys_pause = 0;
	int framerender = 0;
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
	void SimulateGoL();
	void RecalcFreeParticles(bool do_life_dec);
	void CheckStacking();
	void BeforeSim();
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
	int CreateWalls(int x, int y, int rx, int ry, int wall, Brush const *cBrush = nullptr);
	void CreateWallLine(int x1, int y1, int x2, int y2, int rx, int ry, int wall, Brush const *cBrush = nullptr);
	void CreateWallBox(int x1, int y1, int x2, int y2, int wall);
	int FloodWalls(int x, int y, int wall, int bm);

	//Drawing Particles
	int CreateParts(int positionX, int positionY, int c, Brush const &cBrush, int flags = -1);
	int CreateParts(int x, int y, int rx, int ry, int c, int flags = -1);
	int CreatePartFlags(int x, int y, int c, int flags);
	void CreateLine(int x1, int y1, int x2, int y2, int c, Brush const &cBrush, int flags = -1);
	void CreateLine(int x1, int y1, int x2, int y2, int c);
	void CreateBox(int x1, int y1, int x2, int y2, int c, int flags = -1);
	int FloodParts(int x, int y, int c, int cm, int flags = -1);

	void GetGravityField(int x, int y, float particleGrav, float newtonGrav, float & pGravX, float & pGravY);

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

	bool MaxPartsReached() const
	{
		return pfree == -1;
	}

	GravityPtr grav;
	std::unique_ptr<Air> air;

private:
	CoordStack& getCoordStackSingleton();

	void ResetNewtonianGravity(GravityInput newGravIn, GravityOutput newGravOut);
	void DispatchNewtonianGravity();
	void UpdateGravityMask();

	int pfree;
};
