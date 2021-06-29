#pragma once

#include "Config.h"
#include "common/ExplicitSingleton.h"
#include "gui/ModalWindow.h"
#include "simulation/SimulationData.h"
#include "simulation/ElementDefs.h"
#include "simulation/Sample.h"

#include <array>
#include <memory>
#include <list>
#include <map>
#include <set>
#include <vector>

struct SDL_Texture;
class Simulation;
class SimulationRenderer;
class CommandInterface;

namespace gui
{
	class Button;
}

namespace activities
{
namespace game
{
namespace tool
{
	class Tool;
}

namespace brush
{
	class Brush;
}

	class ToolPanel;

	// * The plan is to eventually move this into Simulation.h.
	enum SimAirMode
	{
		simAirModeOn,
		simAirModePressureOff,
		simAirModeVelocityOff,
		simAirModeOff,
		simAirModeNoUpdate,
		simAirModeMax, // * Must be the last enum constant.
	};

	// * The plan is to eventually move this into Simulation.h.
	enum SimGravityMode
	{
		simGravityModeVertical,
		simGravityModeRadial,
		simGravityModeOff,
		simGravityModeMax, // * Must be the last enum constant.
	};

	// * The plan is to eventually move this into Simulation.h.
	enum SimEdgeMode
	{
		simEdgeModeVoid,
		simEdgeModeSolid,
		simEdgeModeLoop,
		simEdgeModeMax, // * Must be the last enum constant.
	};

	// * The plan is to eventually move this into Simulation.h.
	enum SimDecoSpace
	{
		simDecoSpaceSRGB,
		simDecoSpaceLinear,
		simDecoSpaceGamma22,
		simDecoSpaceGamma18,
		simDecoSpaceMax, // * Must be the last enum constant.
	};

	using ActionKey = uint64_t;
	struct Action
	{
		String name, description;
		int freedom;
		int precedence;
		std::function<void ()> begin, end;
	};
	struct ActionContext
	{
		String name, description;
		int precedence;
		std::function<void ()> enter, yield, resume, leave;
		std::map<ActionKey, Action *> actions;
		ActionContext *parent;
		std::vector<ActionContext *> children;
	};
	enum ActionSource
	{
		actionSourceButton,
		actionSourceWheel,
		actionSourceSym,
		actionSourceScan,
		actionSourceMax, // * Must be the last enum constant.
	};

	enum ToolMode
	{
		toolModeNone,
		toolModeFree,
		toolModeLine,
		toolModeRect,
		toolModeFill,
		toolModeMax, // * Must be the last enum constant.
	};

	struct MenuSection
	{
		int icon;
		String name;
	};
	extern std::array<MenuSection, SC_TOTAL> menuSections;

	class Game : public gui::ModalWindow, public ExplicitSingleton<Game>
	{
		SDL_Texture *simulationTexture = NULL;

		std::unique_ptr<Simulation> simulation;
		std::unique_ptr<SimulationRenderer> simulationRenderer;

		gui::Button *sandEffectButton = nullptr;
		gui::Button *drawGravityButton = nullptr;
		gui::Button *drawDecoButton = nullptr;
		gui::Button *newtonianGravityButton = nullptr;
		gui::Button *ambientHeatButton = nullptr;
		gui::Button *pauseButton = nullptr;
		bool sandEffect = false;
		bool drawGravity = false;
		bool drawDeco = false;
		bool newtonianGravity = false;
		bool ambientHeat = false;
		bool legacyHeat = false;
		bool waterEqualisation = false;
		SimAirMode airMode = simAirModeOn;
		float ambientAirTemp = R_TEMP + 273.15f;
		SimGravityMode gravityMode = simGravityModeVertical;
		SimEdgeMode edgeMode = simEdgeModeVoid;
		SimDecoSpace decoSpace = simDecoSpaceSRGB;
		bool perfectCircle = true;
		bool paused = false;
		bool drawHUD = true;
		bool debugHUD = false;

		void InitWindow();

		ToolPanel *toolPanel = nullptr;
		int currentCategory = 0;
		bool shouldBuildToolPanel = true;
		void BuildToolPanel();
		bool stickyCategories;

		std::array<tool::Tool *, 4> currentTools = { { nullptr, nullptr, nullptr, nullptr } };

		std::vector<std::unique_ptr<brush::Brush>> brushes;
		std::unique_ptr<brush::Brush> cellAlignedBrush;
		brush::Brush *lastBrush = nullptr;
		void UpdateLastBrush(int toolIndex);
		int currentBrush = 0;
		void InitBrushes();
		void DrawBrush(void *pixels, int pitch) const;

		std::unique_ptr<CommandInterface> commandInterface;
		struct LogEntry
		{
			String message;
			int32_t pushedAt;
		};
		std::list<LogEntry> log;

		void DrawStatistics() const;
		void DrawSampleInfo() const;
		void DrawLogEntries() const;

		gui::Button *loginButton = nullptr;
		gui::Button *profileButton = nullptr;
		gui::Button *rendererButton = nullptr;
		gui::Component *groupSave = nullptr;
		gui::Component *groupRender = nullptr;
		void UpdateGroups();

		std::vector<std::tuple<int, gui::Button *>> renderModeButtons;
		std::vector<std::tuple<int, gui::Button *>> displayModeButtons;
		std::vector<std::tuple<int, gui::Button *>> colorModeButtons;
		std::vector<std::tuple<int, gui::Button *>> rendererPresetButtons;
		int rendererPreset = 0;
		void ApplyRendererSettings();
		void UpdateRendererSettings();

		std::map<String, std::unique_ptr<Action>> actions;
		std::map<ActionKey, Action *> availableActions;
		std::vector<std::pair<ActionKey, Action *>> activeActions;
		int actionMultiplier = 1;
		bool HandlePress(ActionSource source, int code, int mod, int multiplier);
		bool HandleRelease(ActionSource source, int code);
		void EndAllActions();
		std::map<String, std::unique_ptr<ActionContext>> contexts;
		std::vector<ActionContext *> activeContexts;
		bool shouldRehashAvailableActions = true;
		void ShouldRehashAvailableActions();
		void RehashAvailableActions();
		void InitActions();

		void TogglePaused();
		void ToggleSandEffect();
		void ToggleDrawGravity();
		void ToggleDrawDeco();
		void ToggleNewtonianGravity();
		void ToggleAmbientHeat();
		void CycleAir();
		void CycleGravity();
		void CycleBrush();
		void PreQuit();
		void ToggleDrawHUD();
		void ToggleDebugHUD();

		int activeToolIndex = 0;
		ToolMode activeToolMode = toolModeNone;
		std::map<String, std::unique_ptr<tool::Tool>> tools;
		void InitTools();
		gui::Point toolStartPos;
		void ToolDrawWithBrush(gui::Point from, gui::Point to);

		void AdjustBrushOrZoomSize(int sign);

		SimulationSample simulationSample;

		std::set<String> favorites;

	public:
		Game();
		~Game();
		
		void Tick() final override;
		void Draw() const final override;
		void FocusLose() final override;
		bool MouseMove(gui::Point current, gui::Point delta) final override;
		bool MouseDown(gui::Point current, int button) final override;
		bool MouseUp(gui::Point current, int button) final override;
		bool MouseWheel(gui::Point current, int distance, int direction) final override;
		bool KeyPress(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt) final override;
		bool KeyRelease(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt) final override;
		void RendererUp() final override;
		void RendererDown() final override;

		void Test();

		bool EnterContext(ActionContext *context);
		bool LeaveContext(ActionContext *context);

		void SandEffect(bool newSandEffect);
		bool SandEffect() const
		{
			return sandEffect;
		}

		void DrawGravity(bool newDrawGravity);
		bool DrawGravity() const
		{
			return drawGravity;
		}

		void DrawDeco(bool newDrawDeco);
		bool DrawDeco() const
		{
			return drawDeco;
		}

		void NewtonianGravity(bool newNewtonianGravity);
		bool NewtonianGravity() const
		{
			return newtonianGravity;
		}

		void AmbientHeat(bool newAmbientHeat);
		bool AmbientHeat() const
		{
			return ambientHeat;
		}

		void LegacyHeat(bool newLegacyHeat);
		bool LegacyHeat() const
		{
			return legacyHeat;
		}

		void WaterEqualisation(bool newWaterEqualisation);
		bool WaterEqualisation() const
		{
			return waterEqualisation;
		}

		void AirMode(SimAirMode newAirMode);
		SimAirMode AirMode() const
		{
			return airMode;
		}

		void AmbientAirTemp(float newAmbientAirTemp);
		float AmbientAirTemp() const
		{
			return ambientAirTemp;
		}

		void GravityMode(SimGravityMode newGravityMode);
		SimGravityMode GravityMode() const
		{
			return gravityMode;
		}

		void EdgeMode(SimEdgeMode newEdgeMode);
		SimEdgeMode EdgeMode() const
		{
			return edgeMode;
		}

		void DecoSpace(SimDecoSpace newDecoSpace);
		SimDecoSpace DecoSpace() const
		{
			return decoSpace;
		}

		int CurrentCategory() const
		{
			return currentCategory;
		}

		void CurrentTool(int index, tool::Tool *tool);
		tool::Tool *CurrentTool(int index) const
		{
			return currentTools[index];
		}

		void StickyCategories(bool newStickyCategories)
		{
			stickyCategories = newStickyCategories;
		}
		bool StickyCategories() const
		{
			return stickyCategories;
		}

		void CurrentBrush(int newCurrentBrush);
		int CurrentBrush() const
		{
			return currentBrush;
		}

		void PerfectCircle(bool newPerfectCircle);
		bool PerfectCircle() const
		{
			return perfectCircle;
		}

		void Paused(bool newPaused);
		bool Paused() const
		{
			return paused;
		}

		void DrawHUD(bool newDrawHUD)
		{
			drawHUD = newDrawHUD;
		}
		bool DrawHUD() const
		{
			return drawHUD;
		}

		void DebugHUD(bool newDebugHUD)
		{
			debugHUD = newDebugHUD;
		}
		bool DebugHUD() const
		{
			return debugHUD;
		}

		Simulation *GetSimulation()
		{
			return simulation.get();
		}

		SimulationRenderer *GetRenderer()
		{
			return simulationRenderer.get();
		}

		void ShouldBuildToolPanel();
		void UpdateLoginButton();

		void Log(String message);

		void RendererPreset(int newRendererPreset);
		int RendererPreset() const
		{
			return rendererPreset;
		}

		void RenderMode(int newRenderMode);
		int RenderMode() const;
		void DisplayMode(int newDisplayMode);
		int DisplayMode() const;
		void ColorMode(int newColorMode);
		int ColorMode() const;

		void ToolStart(int index);
		void ToolCancel(int index);
		void ToolFinish(int index);

		void OpenToolSearch();
		void OpenConsole();

		const std::map<String, std::unique_ptr<tool::Tool>> &Tools() const
		{
			return tools;
		}

		void BrushRadius(gui::Point newBrushRadius);
		gui::Point BrushRadius() const;

		void Favorite(const String &identifier, bool newFavorite);
		bool Favorite(const String &identifier) const;

		CommandInterface *GetCommandInterface() const
		{
			return commandInterface.get();
		}
	};
}
}
