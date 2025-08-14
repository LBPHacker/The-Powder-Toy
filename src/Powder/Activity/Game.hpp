#pragma once
#include "graphics/RasterDrawMethods.h"
#include "graphics/RendererFrame.h"
#include "graphics/RendererSettings.h"
#include "simulation/SimulationSettings.h"
#include "Common/Color.hpp"
#include "Common/Point.hpp"
#include "Common/Time.hpp"
#include "Gui/Alignment.hpp"
#include "Gui/InputMapper.hpp"
#include "Gui/TargetAnimation.hpp"
#include "Gui/Text.hpp"
#include "Gui/Ticks.hpp"
#include "Gui/View.hpp"
#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <json/json.h>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

typedef struct SDL_Texture SDL_Texture;

class Brush;
struct CustomGOLData;
class GameSave;
struct RenderableSimulation;
class Renderer;
class Simulation;
struct SimulationSample;
class Snapshot;
struct SnapshotDelta;
class Tool;

namespace Powder::Activity
{
	class OnlineBrowser;

	struct GameToolInfo
	{
		std::unique_ptr<Tool> tool;
		struct TextureInfo
		{
			std::unique_ptr<VideoBuffer> data; // TODO-REDO_UI: persist in the Tool itself
			Gui::View::Pos2 toolAtlasPos{ 0, 0 };
		};
		std::optional<TextureInfo> texture;
	};

	class Game : public Gui::View, public Gui::InputMapper
	{
	public:
		using ToolSlotIndex = int32_t;
		static constexpr ToolSlotIndex toolSlotCount = 10;
		static constexpr Point toolTextureDataSize   = { 26, 14 };

	private:
		using BrushIndex = int32_t;
		std::vector<std::unique_ptr<Brush>> brushes;
		BrushIndex brushIndex = -1;
		Point brushRadius = { 4, 4 };
		void SetBrushIndex(BrushIndex newBrushIndex);
		Brush *GetCurrentBrush();
		const Brush *GetCurrentBrush() const;
		void InitBrushes();

		int32_t toolScroll = 0;
		std::vector<std::optional<GameToolInfo>> tools;
		std::array<Tool *, toolSlotCount> toolSlots{};
		Tool *lastSelectedTool = nullptr;
		Tool *lastHoveredTool = nullptr;
		void ResetToolSlot(ToolSlotIndex toolSlotIndex);
		void InitTools();
		void AllocTool(std::unique_ptr<Tool> tool);
		void FreeTool(Tool *tool);
		using ToolIndex = int32_t;
		std::optional<ToolIndex> GetToolIndex(Tool *tool);
		using ElementIndex = int32_t;
		void AllocElementTool(ElementIndex elementIndex);
		void AllocCustomGolTool(const CustomGOLData &gd);
		void UpdateElementTool(ElementIndex elementIndex);
		bool toolStrengthMul10 = false;
		bool toolStrengthDiv10 = false;
		float GetToolStrength() const;

		bool shouldUpdateToolAtlas = false;
		void UpdateToolTexture(ToolIndex index);
		PlaneAdapter<std::vector<pixel>> toolAtlas;
		void UpdateToolAtlas();
		enum class ExtraToolTexture
		{
			decoSlotBackground,
			max,
		};
		std::array<GameToolInfo::TextureInfo, int32_t(ExtraToolTexture::max)> extraToolTextures;
		SDL_Texture *toolAtlasTexture = nullptr;

		void LoadCustomGol();
		void SaveCustomGol();

		TempScale temperatureScale = TEMPSCALE_CELSIUS;

		using MenuSectionIndex = int32_t;
		MenuSectionIndex activeMenuSection = 0;
		MenuSectionIndex activeMenuSectionBeforeDeco = 0;
		bool menuSectionsNeedClick = false;
		std::vector<Rgba8> decoSlots;
		Rgba8 decoColor = 0xFFFF0000_argb;

		struct LogEntry
		{
			Gui::TargetAnimation<int32_t> alpha;
			std::string message;
		};
		std::deque<LogEntry> logEntries;

		std::unique_ptr<Renderer> rendererRemote;
		Renderer *renderer = nullptr;
		std::optional<FindingElement> GetFindParameters() const;
		RendererSettings rendererSettings;
		enum class RendererThreadState
		{
			absent,
			running,
			paused,
			stopping,
		};
		RendererThreadState rendererThreadState = RendererThreadState::absent;
		std::thread rendererThread;
		std::mutex rendererThreadMx;
		std::condition_variable rendererThreadCv;
		bool rendererThreadOwnsRenderer = false;
		void StartRendererThread();
		void PauseRendererThread();
		void StopRendererThread();
		void RendererThread();
		void WaitForRendererThread();
		void DispatchRendererThread();
		std::unique_ptr<RenderableSimulation> rendererThreadSim;
		std::unique_ptr<RendererFrame> rendererThreadResult;
		RendererStats rendererStats;
		const RendererFrame *rendererFrame = nullptr;
		bool GetRendererThreadEnabled() const;
		void RenderSimulation(const RenderableSimulation &sim, bool handleEvents);
		bool GetDrawDeco() const;
		void SetDrawDeco(bool newDrawDeco);
		bool GetDrawGravity() const;
		void SetDrawGravity(bool newDrawGravity);

		EdgeMode defaultEdgeMode;
		float defaultAmbientAirTemp;
		struct SimContext
		{
			std::unique_ptr<Simulation> simulation;
		};
		SimContext simContextRemote;
		SimContext *simContext = &simContextRemote;
		bool GetSimThreadEnabled() const;
		void PauseSimThread();
		void StartSimThread();
		bool GetSimPaused() const;
		void SetSimPaused(bool newPaused);
		bool GetSandEffect() const;
		void SetSandEffect(bool newSandEffect);
		void ApplySaveParametersToSim(const GameSave &save);
		void UpdateSimUpTo(int upToIndex);
		void BeforeSim();
		void AfterSim();
		bool ReloadSim(bool includePressure);
		void ClearSim();
		void DoSimFrameStep();

		void CycleBrushAction();
		void CycleEdgeModeAction();
		void CycleAirModeAction();
		void ToggleSandEffectAction();
		void ToggleDrawGravityAction();
		void ToggleDrawDecoAction();
		void ToggleNewtonianGravityAction();
		void ToggleAmbientHeatAction();
		void ToggleSimPausedAction();
		void ToggleDebugHudAction();
		void ToggleFindAction();
		void UndoHistoryEntryAction();
		void RedoHistoryEntryAction();
		void ReloadSimAction();
		void GrowGridAction();
		void ShrinkGridAction();
		void ToggleGravityGridAction();
		void ToggleIntroTextAction();
		void ToggleHudAction();
		void ToggleDecoAction();
		void ToggleDecoMenuAction();
		void ResetAmbientHeatAction();
		void ResetAirAction();
		void ResetSparkAction();
		void InvertAirAction();
		void ToggleReplaceAction();
		void ToggleSdeleteAction();

		void ToggleReplaceModeFlag(int flag);

		struct HistoryEntry
		{
			std::unique_ptr<Snapshot> snap;
			std::unique_ptr<SnapshotDelta> delta;

			~HistoryEntry();
		};
		int32_t undoHistoryLimit = 5;
		int32_t historyPosition = 0;
		std::deque<HistoryEntry> history;
		std::unique_ptr<Snapshot> historyCurrent;
		std::unique_ptr<Snapshot> beforeRestore;
		bool UndoHistoryEntry();
		bool RedoHistoryEntry();
		void CreateHistoryEntry();

		Json::Value authors;
		struct LocalSave
		{
			std::string path;
			std::string title;
			std::optional<std::string> error;
			std::unique_ptr<GameSave> gameSave;
		};
		struct OnlineSave
		{
			int32_t id; // TODO: use strings
			int32_t version; // TODO: use strings
			Time createdDate;
			Time updatedDate;
			int32_t votesUp;
			int32_t votesDown;
			int32_t vote;
			bool favourite;
			int32_t commentCount;
			int32_t viewCount;
			std::string author;
			std::string title;
			std::string description;
			bool published;
			std::vector<std::string> tags;
			std::unique_ptr<GameSave> gameSave;
		};
		using Save = std::variant<
			LocalSave,
			OnlineSave
		>;
		bool loadWithoutPressure = false;
		std::optional<Save> save;
		void SetSave(std::optional<Save> newSave, bool includePressure);
		void LoadGameSave(const GameSave &gameSave, bool includePressure);

		void BeforeSimDraw();
		void AfterSimDraw();

		void InitActions();

		enum class DrawMode
		{
			free,
			line,
			rect,
			fill,
		};
		struct DrawState
		{
			DrawMode mode;
			ToolSlotIndex toolSlotIndex;
			Pos2 initialMousePos;
			bool mouseMovedSinceLastTick = false;
		};
		std::optional<DrawState> drawState;
		void BeginBrush(ToolSlotIndex toolSlotIndex, DrawMode drawMode);
		bool DragBrush(bool invokeToolDrag);
		void EndBrush(ToolSlotIndex toolSlotIndex);
		void AdjustBrushSize(int32_t diffX, int32_t diffY, bool logarithmic);

		bool hud = true;
		bool debugHud = false;
		void GetBasicSampleText(std::vector<std::string> &lines, std::optional<int> &wavelengthGfx, const SimulationSample &sample);
		void GetDebugSampleText(std::vector<std::string> &lines, const SimulationSample &sample);
		void GetFpsLines(std::vector<std::string> &lines, const SimulationSample &sample);
		void DrawHudLines(const std::vector<std::string> &lines, std::optional<int> wavelengthGfx, Gui::Alignment alignment, Rgb8 color);
		void DrawLogLines();
		void GuiHud();

		struct ToolTipInfo
		{
			std::string message;
			Pos2 pos;
			Gui::Alignment horizontalAlignment;
			Gui::Alignment verticalAlignment;
		};
		Gui::TargetAnimation<int32_t> toolTipAlpha;
		bool toolTipQueued = false;
		std::optional<ToolTipInfo> toolTipInfo;
		void QueueToolTip(std::string message, Pos2 pos, Gui::Alignment horizontalAlignment, Gui::Alignment verticalAlignment);
		void DrawToolTip();

		struct InfoTipInfo
		{
			std::string message;
		};
		Gui::TargetAnimation<int32_t> infoTipAlpha;
		std::optional<InfoTipInfo> infoTipInfo;
		void QueueInfoTip(std::string message);
		void DrawInfoTip();

		std::string introText;
		Gui::TargetAnimation<int32_t> introTextAlpha;
		void DrawIntroText();
		void DismissIntroText();

		SDL_Texture *simTexture = nullptr;
		void DrawSim();
		void DrawBrush(void *pixels, int pitch);

		void GuiSim();
		void GuiTools();
		void GuiRight();
		void GuiBottom();
		void GuiSave();
		void GuiRenderer();
		void GuiQuickOptions();
		void GuiMenuSections();

		void OpenElementSearch();
		void OpenProperty();

		void HandleBeforeFrame() final override;
		bool HandleEvent(const SDL_Event &event) final override;
		void HandleTick() final override;
		void HandleBeforeGui() final override;
		void Gui() final override;
		void HandleAfterGui() final override;
		void HandleAfterFrame() final override;
		void SetRendererUp(bool newRendererUp) final override;
		void SetOnTop(bool newOnTop) final override;

		bool sampleProperty = false;

		bool showRenderer = false;
		void UseRendererPreset(int index);

		OnlineBrowser *onlineBrowser = nullptr;
		std::shared_ptr<Gui::Stack> onlineBrowserStack;
		void OpenOnlineBrowser();

	public:
		Game(Gui::Host &newHost);
		~Game();

		void VisualLog(std::string message);

		void SetOnlineBrowserStack(std::shared_ptr<Gui::Stack> newOnlineBrowserStack)
		{
			onlineBrowserStack = newOnlineBrowserStack;
		}

		void SetBrowser(OnlineBrowser *newOnlineBrowser)
		{
			onlineBrowser = newOnlineBrowser;
		}

		const std::vector<std::optional<GameToolInfo>> &GetTools() const
		{
			return tools;
		}
		bool GuiToolButton(Gui::View &view, const GameToolInfo &info, SDL_Texture *externalToolAtlasTexture) const; // TODO-REDO_UI: make static, add toolSlots param
		void UpdateToolAtlasTexture(Gui::View &view, bool rendererUp, SDL_Texture *&externalToolAtlasTexture) const; // TODO-REDO_UI: factor out ToolAtlas
		void SelectTool(ToolSlotIndex toolSlotIndex, Tool *tool);
		bool GetShowingDecoTools() const;
		Tool *GetToolFromIdentifier(const ByteString &identifier);
		std::optional<Rgb8> GetColorUnderMouse() const;

		bool GetSampleProperty() const
		{
			return sampleProperty;
		}

		const RendererFrame &GetRendererFrame() const
		{
			return *rendererFrame;
		}

		Rgba8 GetDecoColor() const
		{
			return decoColor;
		}
		void SetDecoColor(Rgba8 newDecoColor)
		{
			decoColor = newDecoColor;
		}

		Simulation &GetSimulation()
		{
			return *simContext->simulation.get();
		}

		TempScale GetTemperatureScale() const
		{
			return temperatureScale;
		}
		void SetTemperatureScale(TempScale newTemperatureScale)
		{
			temperatureScale = newTemperatureScale;
		}

		Pos2 ResolveZoom(Pos2 point) const
		{
			return point; // TODO-REDO_UI
		}

		bool GetHeat() const;
		void SetHeat(bool newHeat);
		bool GetNewtonianGravity() const;
		void SetNewtonianGravity(bool newNewtonianGravity);
		bool GetAmbientHeat() const;
		void SetAmbientHeat(bool newAmbientHeat);
		bool GetWaterEqualization() const;
		void SetWaterEqualization(bool newWaterEqualization);
		AirMode GetAirMode() const;
		void SetAirMode(AirMode newAirMode);
		GravityMode GetGravityMode() const;
		void SetGravityMode(GravityMode newGravityMode);
		EdgeMode GetEdgeMode() const;
		void SetEdgeMode(EdgeMode newEdgeMode);
		float GetAmbientAirTemp() const;
		void SetAmbientAirTemp(float newAmbientAirTemp);
	};
}
