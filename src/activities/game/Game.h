#pragma once
#include "Config.h"

#include "common/ExplicitSingleton.h"
#include "common/Path.h"
#include "common/tpt-compat.h"
#include "gui/ModalWindow.h"
#include "gui/LinearDelayAnimation.h"
#include "simulation/SimulationData.h"
#include "simulation/ElementDefs.h"
#include "simulation/Sample.h"
#include "simulation/SimTool.h"
#include "backend/SaveInfo.h"
#include "json/json.h"

#include <array>
#include <memory>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <deque>

struct SDL_Texture;
class Simulation;
class GameSave;
class Snapshot;
struct SnapshotDelta;
class SimulationRenderer;
class CommandInterface;

namespace backend
{
	class ExecVote;
}

namespace graphics
{
	class ThumbnailRendererTask;
}

namespace gui
{
	class Button;
	class Texture;
}

namespace activities::browser
{
	class OnlineBrowser;
	class LocalBrowser;
}

namespace activities::console
{
	class Console;
}

namespace activities::game::tool
{
	class Tool;
}

namespace activities::game::brush
{
	class Brush;
}

namespace activities::game
{
	class ToolPanel;

	struct HistoryEntry
	{
		std::unique_ptr<Snapshot> snap;
		std::unique_ptr<SnapshotDelta> delta;

		~HistoryEntry();
	};

	// * TODO-REDO_UI: Move this into Simulation.h.
	enum SimAirMode
	{
		simAirModeOn,
		simAirModePressureOff,
		simAirModeVelocityOff,
		simAirModeOff,
		simAirModeNoUpdate,
		simAirModeMax, // * Must be the last enum constant.
	};

	// * TODO-REDO_UI: Move this into Simulation.h.
	enum SimGravityMode
	{
		simGravityModeVertical,
		simGravityModeRadial,
		simGravityModeOff,
		simGravityModeMax, // * Must be the last enum constant.
	};

	// * TODO-REDO_UI: Move this into Simulation.h.
	enum SimEdgeMode
	{
		simEdgeModeVoid,
		simEdgeModeSolid,
		simEdgeModeLoop,
		simEdgeModeMax, // * Must be the last enum constant.
	};

	// * TODO-REDO_UI: Move this into Simulation.h.
	enum SimDecoSpace
	{
		simDecoSpaceSRGB,
		simDecoSpaceLinear,
		simDecoSpaceGamma22,
		simDecoSpaceGamma18,
		simDecoSpaceMax, // * Must be the last enum constant.
	};

	struct Action
	{
		String name, description;
		uint32_t dof; // * Degrees of freedom.
		int precedence;
		std::function<void ()> begin;
		std::function<void (bool)> end;
	};

	struct ActionContext
	{
		String name, description;
		int precedence = 0;
		std::function<bool ()> canEnter;
		std::function<void ()> enter, yield, resume, leave;
		std::map<uint64_t, Action *> actions;
		ActionContext *parent = nullptr;
		std::vector<ActionContext *> children;
		bool active = false;
	};

	enum ActionSource
	{
		actionSourceButton,
		actionSourceWheel,
		// actionSourceSym, // * I don't think we need sym-based shortcuts anymore.
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

	enum SelectMode
	{
		selectModeCopy,
		selectModeCut,
		selectModeStamp,
		selectModeMax, // * Must be the last enum constant.
	};

	struct MenuSection
	{
		int icon;
		String name;
	};
	extern std::array<MenuSection, SC_TOTAL> menuSections;

	struct LockedTextureData
	{
		void *pixels;
		int pitch;

		uint32_t &operator ()(gui::Point point) const
		{
			return *reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(pixels) + point.y * pitch + point.x * sizeof(uint32_t));
		}
	};

	class Game : public gui::ModalWindow, public common::ExplicitSingleton<Game>
	{
		std::shared_ptr<browser::OnlineBrowser> onlineBrowser;
		std::shared_ptr<browser::LocalBrowser> localBrowser;
		std::shared_ptr<console::Console> cons;

		SDL_Texture *simulationTexture = NULL;

		std::shared_ptr<GameSave> clipboard;
		std::unique_ptr<Snapshot> beforeRestore;
		std::deque<HistoryEntry> history;
		std::unique_ptr<Snapshot> historyCurrent;
		int historyPosition = 0;
		int historyLimit = 0;

		std::unique_ptr<Simulation> simulation;
		std::unique_ptr<SimulationRenderer> simulationRenderer;

		gui::Button *prettyPowdersButton = nullptr;
		gui::Button *drawGravityButton = nullptr;
		gui::Button *drawDecoButton = nullptr;
		gui::Button *newtonianGravityButton = nullptr;
		gui::Button *ambientHeatButton = nullptr;
		gui::Button *pauseButton = nullptr;
		bool prettyPowders = false;
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
		bool includePressure = true;
		bool paused = false;
		bool drawHUD = true;
		bool debugHUD = false;

		void InitWindow();
		void LoadPreferences();

		ToolPanel *toolPanel = nullptr;
		int currentMenuSection = 0;
		bool shouldBuildToolPanel = true;
		void BuildToolPanelIfNeeded();
		bool stickyCategories;

		void DrawHighlight(const LockedTextureData ltd, gui::Point point) const;
		void DrawHighlightRect(const LockedTextureData ltd, gui::Rect rect, gui::Point origin) const;

		std::array<tool::Tool *, 4> currentTools = {};

		std::vector<std::unique_ptr<brush::Brush>> brushes;
		std::unique_ptr<brush::Brush> cellAlignedBrush;
		const brush::Brush *lastBrush = nullptr;
		void UpdateLastBrush(int toolIndex);
		int currentBrush = 0;
		void InitBrushes();
		void DrawBrush(const LockedTextureData ltd) const;

		std::unique_ptr<CommandInterface> commandInterface;
		struct LogEntry
		{
			String message;
			int32_t pushedAt;
		};
		std::list<LogEntry> log;
		void InitCommandInterface();

		void DrawStatistics() const;
		void DrawSampleInfo() const;
		void DrawLogEntries() const;

		gui::Button *loginButton = nullptr;
		gui::Button *profileButton = nullptr;
		gui::Button *rendererButton = nullptr;
		gui::Component *groupSave = nullptr;
		gui::Component *groupRender = nullptr;
		void UpdateGroups();

		std::vector<std::tuple<uint32_t, gui::Button *>> renderModeButtons;
		std::vector<std::tuple<uint32_t, gui::Button *>> displayModeButtons;
		std::vector<std::tuple<uint32_t, gui::Button *>> colorModeButtons;
		std::vector<std::tuple<int, gui::Button *>> rendererPresetButtons;
		int rendererPreset = 0;
		void ApplyRendererSettings();
		void UpdateRendererSettings();

		std::map<String, std::unique_ptr<Action>> actions;
		std::map<uint64_t, Action *> availableActions;
		std::vector<std::pair<uint64_t, Action *>> activeActions;
		int actionMultiplier = 1;
		bool HandlePress(ActionSource source, int code, int mod, int multiplier);
		bool HandleRelease(ActionSource source, int code);
		void EndAllActions();
		std::map<String, std::unique_ptr<ActionContext>> contexts;
		std::vector<ActionContext *> activeContexts;
		bool shouldRehashAvailableActions = true;
		bool inContextCallback = false;
		void RehashAvailableActionsIfNeeded();
		void InitActions();

		void TogglePaused();
		void TogglePrettyPowders();
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

		std::vector<SimTool> simulationTools;

		int activeToolIndex = 0;
		ToolMode activeToolMode = toolModeNone;
		std::map<String, std::unique_ptr<tool::Tool>> tools;
		void InitTools();
		gui::Point toolOrigin;
		void ToolDrawLine(gui::Point from, gui::Point to);

		void AdjustBrushOrZoomSize(int sign);

		SimulationSample simulationSample;

		std::set<String> favorites;

		void ActivateActionTip(const String &text);
		void DeectivateActionTip();
		void DrawActionTip() const;
		String actionTipText;
		gui::LinearDelayAnimation actionTipAnim;

		void ToggleIntroText();
		void DrawIntroText() const;
		String introText;
		enum IntroTextState
		{
			introTextInitial,
			introTextForcedOn,
			introTextForcedOff,
		};
		IntroTextState introTextState = introTextInitial;
		gui::LinearDelayAnimation introTextAnim;

		SelectMode nextSelectMode = selectModeMax;
		SelectMode selectMode = selectModeMax;
		ActionContext *contextSelect = nullptr;
		void SelectStart(SelectMode newSelectMode);
		void SelectSetOrigin();
		void SelectFinish(bool cancel);
		void SelectCancel();
		gui::Point selectOrigin;
		bool selectOriginValid = false;
		void DrawSelectArea(const LockedTextureData ltd) const;
		void DrawSelectBlindfold() const;

		ActionContext *contextPaste = nullptr;
		gui::Point pasteOffset;
		uint32_t pasteRotflip;
		gui::Point pasteNudge;
		gui::Point pasteSize;
		gui::Texture *pasteTexture = nullptr;
		std::shared_ptr<graphics::ThumbnailRendererTask> pasteRender;
		std::shared_ptr<GameSave> nextPasteSource;
		std::shared_ptr<GameSave> pasteSource;
		std::shared_ptr<GameSave> reloadSource;
		backend::SaveInfo reloadSourceInfo = {};
		void PasteDispatchRender();
		void PasteGetThumbnail();
		void PasteFromPrevStamp();
		void PasteFromNextStamp();
		void PasteFromClipboard();
		void PasteStart(std::shared_ptr<GameSave> newPasteSource);
		void PasteFinish(bool cancel);
		void PasteCancel();
		void PasteNudge(gui::Point nudge);
		void PasteCenter();
		void PasteRotate();
		void PasteHorizontalFlip();
		void PasteVerticalFlip();
		void DrawPasteArea(const LockedTextureData ltd) const;
		void DrawPasteTexture() const;
		gui::Point PastePos() const;
		gui::Point PasteSize() const;
		gui::Rect PasteDestination() const;

		enum OpenButtonAction
		{
			openButtonOnline,
			openButtonLocal,
			openButtonMax, // * Must be the last enum constant.
		};
		OpenButtonAction GetOpenButtonAction() const;
		gui::Button *openButton = nullptr;
		enum ReloadButtonAction
		{
			reloadButtonNone,
			reloadButtonLocal,
			reloadButtonOnline,
			reloadButtonPreview,
			reloadButtonMax, // * Must be the last enum constant.
		};
		ReloadButtonAction GetReloadButtonAction() const;
		gui::Button *reloadButton = nullptr;
		enum UpdateButtonAction
		{
			updateButtonNone,
			updateButtonLocal,
			updateButtonOnline,
			updateButtonMax, // * Must be the last enum constant.
		};
		UpdateButtonAction GetUpdateButtonAction() const;
		gui::Button *updateButton = nullptr;
		enum SaveButtonAction
		{
			saveButtonLocal,
			saveButtonForceLocal,
			saveButtonOnline,
			saveButtonRename,
			saveButtonMax, // * Must be the last enum constant.
		};
		SaveButtonAction GetSaveButtonAction() const;
		gui::Button *saveButton = nullptr;
		bool saveControlModState = false;
		void UpdateSaveControls();

		gui::Button *tagsButton = nullptr;
		void UpdateTagControls();

		gui::Button *likeButton = nullptr;
		gui::Button *dislikeButton = nullptr;
		std::shared_ptr<backend::ExecVote> execVote;
		int execVoteDirection;
		void ExecVoteStart(int direction);
		void ExecVoteFinish();
		void UpdateVoteControls();

		void UpdateOnlineControls();

		void LoadSave(std::shared_ptr<GameSave> save);
		void ClearSimulation();
		void ReloadSimulation();

		Json::Value authors;

		Path savesPath;
		Path stampsPath;

		uint32_t decoColor = 0;

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

		bool EnterContext(ActionContext *context);
		bool LeaveContext(ActionContext *context);

		void PrettyPowders(bool newPrettyPowders);
		bool PrettyPowders() const
		{
			return prettyPowders;
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

		const std::vector<SimTool> &SimulationTools() const
		{
			return simulationTools;
		}

		int CurrentMenuSection() const
		{
			return currentMenuSection;
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

		void IncludePressure(bool newIncludePressure);
		bool IncludePressure() const
		{
			return includePressure;
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

		void ShouldBuildToolPanel()
		{
			shouldBuildToolPanel = true;
		}
		void UpdateLoginControls();

		void Log(String message);

		void RendererPreset(int newRendererPreset);
		int RendererPreset() const
		{
			return rendererPreset;
		}

		void RenderMode(uint32_t newRenderMode);
		uint32_t RenderMode() const;
		void DisplayMode(uint32_t newDisplayMode);
		uint32_t DisplayMode() const;
		void ColorMode(uint32_t newColorMode);
		uint32_t ColorMode() const;

		void ToolStart(int index);
		void ToolCancel(int index);
		void ToolFinish(int index);

		void OpenToolSearch();
		void OpenConsole();

		const std::map<String, std::unique_ptr<tool::Tool>> &Tools() const
		{
			return tools;
		}

		const std::vector<std::unique_ptr<brush::Brush>> &Brushes() const
		{
			return brushes;
		}

		void BrushRadius(gui::Point newBrushRadius);
		gui::Point BrushRadius() const;

		void Favorite(const String &identifier, bool newFavorite);
		bool Favorite(const String &identifier) const;

		CommandInterface *GetCommandInterface() const
		{
			return commandInterface.get();
		}

		const Snapshot *HistoryCurrent() const;
		bool HistoryCanUndo() const
		{
			return historyPosition > 0;
		}
		void HistoryUndo();
		bool HistoryCanRedo() const
		{
			return historyPosition < int(history.size());
		}
		void HistoryRedo();
		void HistorySnapshot();

		void HistoryLimit(int newHistoryLimit);
		int HistoryLimit() const
		{
			return historyLimit;
		}

		void LoadLocalSave(String name, std::shared_ptr<GameSave> save);
		void LoadOnlineSave(backend::SaveInfo info, std::shared_ptr<GameSave> save);

		
		void SavesPath(Path newSavesPath)
		{
			savesPath = newSavesPath;
		}
		const Path &SavesPath()
		{
			return savesPath;
		}
		
		void StampsPath(Path newStampsPath)
		{
			stampsPath = newStampsPath;
		}
		const Path &StampsPath()
		{
			return stampsPath;
		}
		
		void DecoColor(uint32_t newDecoColor)
		{
			decoColor = newDecoColor;
		}
		uint32_t DecoColor()
		{
			return decoColor;
		}
	};
}
