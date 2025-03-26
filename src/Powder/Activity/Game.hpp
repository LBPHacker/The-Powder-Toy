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
#include "common/Bson.h"
#include "client/ServerNotification.h"
#include "lua/CommandInterfacePtr.h"
#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <future>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <variant>
#include <vector>

constexpr auto MOUSEUP_NORMAL  = INT32_C(0);
constexpr auto MOUSEUP_BLUR    = INT32_C(1);
constexpr auto MOUSEUP_DRAWEND = INT32_C(2);

constexpr auto DEBUG_PARTS      = UINT32_C(0x00000001);
constexpr auto DEBUG_ELEMENTPOP = UINT32_C(0x00000002);
constexpr auto DEBUG_LINES      = UINT32_C(0x00000004);
constexpr auto DEBUG_PARTICLE   = UINT32_C(0x00000008);
constexpr auto DEBUG_SURFNORM   = UINT32_C(0x00000010);
constexpr auto DEBUG_SIMHUD     = UINT32_C(0x00000020);
constexpr auto DEBUG_RENHUD     = UINT32_C(0x00000040);
constexpr auto DEBUG_AIRVEL     = UINT32_C(0x00000080);

typedef struct SDL_Texture SDL_Texture;

class Brush;
struct CustomGOLData;
class GameSave;
struct RenderableSimulation;
class Renderer;
class SaveFile;
class SaveInfo;
class Simulation;
struct SimulationSample;
class Snapshot;
struct SnapshotDelta;
class Tool;

namespace http
{
	class ExecVoteRequest;
}

namespace Powder
{
	class ThreadPool;
	class Stacks;
}

namespace Powder::Gui
{
	class StaticTexture;
}

namespace Powder::Activity
{
	class Console;
	class OnlineBrowser;
	class LocalBrowser;
	class StampBrowser;

	struct GameToolInfo
	{
		std::unique_ptr<Tool> tool;
		struct TextureInfo
		{
			PlaneAdapter<std::vector<pixel>> data; // TODO-REDO_UI-POSTCLEANUP: persist in the Tool itself
			Gui::View::Pos2 toolAtlasPos{ 0, 0 };
		};
		std::optional<TextureInfo> texture;
		bool favorite = false;
	};

	class Game : public Gui::View, public Gui::InputMapper
	{
		ThreadPool &threadPool;
		std::shared_ptr<ActionContext> rootContext;

	public:
		enum class SelectMode
		{
			copy,
			cut,
			stamp,
		};
		enum class ScreenshotFormat
		{
			png,
			bmp,
			ppm,
		};

		struct ZoomMetrics
		{
			Pos2 to{ 0, 0 };
			int32_t scale = 1;
			Rect from{ { 0, 0 }, { 31, 31 } };
		};

		using ToolSlotIndex = int32_t;
		static constexpr ToolSlotIndex toolSlotCount = 10;
		static constexpr Point toolTextureDataSize   = { 26, 14 };

	private:
		using BrushIndex = int32_t;
		std::vector<std::unique_ptr<Brush>> brushes;
		BrushIndex brushIndex = -1;
		Point brushRadius = { 4, 4 };
		Brush *GetCurrentBrush();
		const Brush *GetCurrentBrush() const;
		bool perfectCircleBrush = true;
		void InitBrushes();

		int32_t toolScroll = 0;
		std::vector<std::optional<GameToolInfo>> tools;
		std::array<Tool *, toolSlotCount> toolSlots{};
		Tool *lastSelectedTool = nullptr;
		Tool *lastHoveredTool = nullptr;
		void ResetToolSlot(ToolSlotIndex toolSlotIndex);
		void InitTools();
		using ToolIndex = int32_t;
		using ElementIndex = int32_t;
		Tool *AllocCustomGolTool(const CustomGOLData &gd);
		bool toolStrengthMul10 = false;
		bool toolStrengthDiv10 = false;
		float GetToolStrength() const;

		bool shouldUpdateToolAtlas = true;
		void UpdateToolTexture(ToolIndex index);
		PlaneAdapter<std::vector<pixel>> toolAtlas;
		void UpdateToolAtlas();
		enum class ExtraToolTexture
		{
			decoSlotBackground,
			max,
		};
		std::array<GameToolInfo::TextureInfo, int32_t(ExtraToolTexture::max)> extraToolTextures;
		std::unique_ptr<Gui::StaticTexture> toolAtlasTexture;

		void LoadCustomGol();
		void SaveCustomGol() const;

		TempScale temperatureScale = TEMPSCALE_CELSIUS;

		using MenuSectionIndex = int32_t;
		MenuSectionIndex activeMenuSectionBeforeDeco = 0;
		bool menuSectionsNeedClick = false;
		std::vector<Rgba8> decoSlots;
		Rgba8 decoColor = 0xFFFF0000_argb;

		bool shouldUpdateDecoTools = true;
		void UpdateDecoTools();

		struct LogEntry
		{
			Gui::TargetAnimation<int32_t> alpha;
			std::string message;
		};
		std::deque<LogEntry> logEntries;

		struct NotificationEntry
		{
			std::string message;
			std::function<void ()> action;
		};
		std::vector<NotificationEntry> notificationEntries;
		void GuiNotifications();

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
		time_t lastScreenshotTime;
		int32_t screenshotIndex = 0;
		bool GetRendererThreadEnabled() const;
		void RenderSimulation(const RenderableSimulation &sim, bool handleEvents);
		bool GetDrawGravity() const;
		void SetDrawGravity(bool newDrawGravity);

		EdgeMode defaultEdgeMode;
		float defaultAmbientAirTemp;
		std::unique_ptr<Simulation> simulation;
		bool simPaused = false;
		int queuedFrames = 0;
		bool IsSimRunning() const;
		bool GetSandEffect() const;
		void SetSandEffect(bool newSandEffect);
		void ApplySaveParametersToSim(const GameSave &save);
		void BeforeSim();
		void AfterSim();
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
		void InvertAirAction();
		void ToggleReplaceAction();
		void ToggleSdeleteAction();
		void QuitAction();
		void PasteAction();
		void CopyAction();
		void CutAction();
		void StampAction();
		void LoadLastStampAction();
		void TakeScreenshotAction();
		void InstallAction();
		void ToggleFullscreenAction();

		std::shared_ptr<Console> console;

		void ToggleReplaceModeFlag(int32_t flag);

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

		Bson authors;
		using LocalSave = std::unique_ptr<SaveFile>;
		using OnlineSave = std::unique_ptr<SaveInfo>;
		using Save = std::variant<
			LocalSave,
			OnlineSave
		>;
		bool invertIncludePressure = false;
		std::optional<Save> save;
		void SetSaveInternal(std::optional<Save> newSave, bool includePressure, bool resetExecVote);
		void LoadGameSave(const GameSave &gameSave, bool includePressure);

		std::unique_ptr<http::ExecVoteRequest> execVoteRequest;
		void BeginVote(int32_t vote);

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
			bool affectedByZoom;
			bool mouseMovedSinceLastTick = false;
		};
		std::optional<DrawState> drawState;
		void ResetDrawStateIfZoomChanged();
		void BeginBrush(ToolSlotIndex toolSlotIndex, DrawMode drawMode);
		bool DragBrush(bool invokeToolDrag);
		void EndBrush(ToolSlotIndex toolSlotIndex);
		void AdjustBrushSize(int32_t diffX, int32_t diffY, bool logarithmic);

		void GetBasicSampleText(std::vector<std::string> &lines, std::optional<int32_t> &wavelengthGfx, const SimulationSample &sample);
		void GetDebugSampleText(std::vector<std::string> &lines, const SimulationSample &sample);
		void GetFpsLines(std::vector<std::string> &lines, const SimulationSample &sample);
		void DrawHudLines(const std::vector<std::string> &lines, std::optional<int32_t> wavelengthGfx, Gui::Alignment alignment, Rgb8 color);
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
		struct DrawSimFrame : public RasterDrawMethods<DrawSimFrame> // TODO-REDO_UI-POSTCLEANUP: resize to RES.X * RES.Y
		{
			RendererFrame video;

			Rect GetClipRect() const // TODO-REDO_UI-POSTCLEANUP: remove
			{
				return RES.OriginRect();
			}
		};
		DrawSimFrame drawSimFrame;
		void DrawSim();
		void DrawBrush();
		void DrawZoomRect();
		void DrawSelect();

		void GuiZoom();
		void GuiSim();
		void GuiSimSigns();
		void GuiTools();
		void GuiRight();
		void GuiBottom();
		void GuiSave();
		void GuiRenderer();
		void GuiQuickOptions();
		void GuiMenuSections();

		void OpenElementSearch();
		void OpenProperty();

		bool HandleEvent(const SDL_Event &event) final override;
		void HandleTick() final override;
		void HandleBeforeGui() final override;
		void Gui() final override;
		void HandleAfterGui() final override;
		void SetRendererUp(bool newRendererUp) final override;
		void SetOnTop(bool newOnTop) final override;
		bool Cancel() final override;
		void Exit() final override;
		bool GetWantTextInput() const final override;

		bool sampleProperty = false;
		bool saveButtonsAltFunction = false;

		bool showRenderer = false;

		OnlineBrowser *onlineBrowser = nullptr;
		Gui::Stack *onlineBrowserStack = nullptr;
		void OpenOnlineBrowser();
		void OpenOnlinePreview();

		LocalBrowser *localBrowser = nullptr;
		Gui::Stack *localBrowserStack = nullptr;
		void OpenLocalBrowser();

		StampBrowser *stampBrowser = nullptr;
		Gui::Stack *stampBrowserStack = nullptr;
		void OpenStampBrowser();

		Gui::Stack *gameStack = nullptr;
		Stacks *stacks = nullptr;

		std::shared_ptr<ActionContext> placeZoomContext;
		void AdjustZoomSize(int32_t diffX, int32_t diffY, bool logarithmic);
		ZoomMetrics MakeZoomMetrics(Size2 size) const;
		std::optional<Pos2> GetSimMousePos() const;

		CommandInterfacePtr commandInterface;

		std::shared_ptr<ActionContext> pasteContext;
		std::shared_ptr<Action> pasteFinish;
		struct PasteSave
		{
			std::unique_ptr<GameSave> original;
			std::unique_ptr<GameSave> transformed;
			Mat2x2 transform = Mat2x2::Identity;
			Pos2 translate = Pos2::Zero;
			std::future<std::unique_ptr<VideoBuffer>> thumbnailFuture;
			std::unique_ptr<VideoBuffer> thumbnail;
		};
		std::optional<PasteSave> pasteSave;
		Pos2 GetPlaceSavePos(Pos2 pos) const;
		void EndPaste(Pos2 pos, bool includePressure);
		void ApplyPasteTransform();
		void TranslatePasteSave(Pos2 addToTranslate);
		void TransformPasteSave(Mat2x2 mulToTransform);

		std::shared_ptr<ActionContext> selectContext;
		std::shared_ptr<Action> selectFinish;
		std::optional<SelectMode> selectMode;
		std::optional<Pos2> selectFirstPos;
		void EndSelectAction(Pos2 first, Pos2 second, SelectMode mode, bool includePressure);

		std::optional<int32_t> lastHoveredSign;
		bool ClickSign(ToolSlotIndex toolSlotIndex);

		void DropFile(ByteString file);

	public:
		Game(Gui::Host &newHost, ThreadPool &newThreadPool);
		~Game();

		void VisualLog(std::string message);
		void Log(std::string message);

		void SetStacks(Stacks *newStacks) { stacks = newStacks; }
		void SetGameStack(Gui::Stack *newGameStack) { gameStack = newGameStack; }
		void SetOnlineBrowserStack(Gui::Stack *newOnlineBrowserStack) { onlineBrowserStack = newOnlineBrowserStack; }
		void SetOnlineBrowser(OnlineBrowser *newOnlineBrowser) { onlineBrowser = newOnlineBrowser; }
		void SetLocalBrowserStack(Gui::Stack *newLocalBrowserStack) { localBrowserStack = newLocalBrowserStack; }
		void SetLocalBrowser(LocalBrowser *newLocalBrowser) { localBrowser = newLocalBrowser; }
		void SetStampBrowserStack(Gui::Stack *newStampBrowserStack) { stampBrowserStack = newStampBrowserStack; }
		void SetStampBrowser(StampBrowser *newStampBrowser) { stampBrowser = newStampBrowser; }

		ThreadPool &GetThreadPool() const { return threadPool; }

		const std::vector<std::optional<GameToolInfo>> &GetTools() const { return tools; }
		bool GuiToolButton(Gui::View &view, const GameToolInfo &info, Gui::StaticTexture &externalToolAtlasTexture, bool indicateFavorite) const;
		bool GetShowingDecoTools() const;
		std::optional<Rgb8> GetColorUnderMouse() const;

		std::set<ByteString> favorites;
		void ToggleFavorite(Tool *tool);
		void LoadFavorites();
		void SaveFavorites() const;

		bool GetSampleProperty() const { return sampleProperty; }

		const RendererFrame &GetRendererFrame() const { return *rendererFrame; }

		Simulation &GetSimulation();
		Renderer &GetRenderer();

		TempScale GetTemperatureScale() const { return temperatureScale; }
		void SetTemperatureScale(TempScale newTemperatureScale) { temperatureScale = newTemperatureScale; }

		std::optional<Pos2> ResolveZoomIfAffected(Pos2 point) const;

		bool GetHeat() const;
		void SetHeat(bool newHeat);
		bool GetNewtonianGravity() const;
		void SetNewtonianGravity(bool newNewtonianGravity);
		bool GetAmbientHeat() const;
		void SetAmbientHeat(bool newAmbientHeat);
		bool GetVorticityCoeff() const;
		void SetVorticityCoeff(bool newVorticityCoeff);
		bool GetWaterEqualization() const;
		void SetWaterEqualization(bool newWaterEqualization);
		AirMode GetAirMode() const;
		void SetAirMode(AirMode newAirMode);
		GravityMode GetGravityMode() const;
		void SetGravityMode(GravityMode newGravityMode);
		float GetAmbientAirTemp() const;
		void SetAmbientAirTemp(float newAmbientAirTemp);
		void ResetSparkAction();

		void SetSave(std::optional<Save> newSave, bool includePressure);

		bool GetIncludePressure() const;

		std::unique_ptr<Gui::StaticTexture> MakeToolAtlasTexture(View &view) const;
		void AddNotification(NotificationEntry notificationEntry);
		void AddServerNotification(ServerNotification serverNotification);

		CommandInterface &GetCommandInterface() const { return *commandInterface; }

		void OpenOnlinePreview(int32_t id, int32_t version, bool quickOpen);
		void UseRendererPreset(int32_t index);
		void ClearSim();
		bool ReloadSim(bool includePressure);
		bool UndoHistoryEntry();
		bool RedoHistoryEntry();
		void CreateHistoryEntry();

		Tool *AllocTool(std::unique_ptr<Tool> tool);
		void AllocElementTool(ElementIndex elementIndex);
		void UpdateElementTool(ElementIndex elementIndex);
		void FreeTool(Tool *tool);
		Tool *GetToolFromIdentifier(const ByteString &identifier);
		Tool *GetToolFromIndex(ToolIndex index);
		std::optional<ToolIndex> GetIndexFromTool(const Tool *tool);
		void SelectTool(ToolSlotIndex toolSlotIndex, Tool *tool);
		Tool *GetSelectedTool(ToolSlotIndex toolSlotIndex) const;

		BrushIndex GetBrushIndex() const { return brushIndex; }
		Point GetBrushRadius() const { return brushRadius; }
		std::optional<BrushIndex> GetIndexFromBrush(const Brush *brush);
		void SetBrushRadius(Point newBrushRadius);
		void SetBrushIndex(BrushIndex newBrushIndex);
		bool GetPerfectCircleBrush() const { return perfectCircleBrush; }
		void SetPerfectCircleBrush(bool newPerfectCircleBrush);
		Rgba8 GetDecoColor() const { return decoColor; }
		void SetDecoColor(Rgba8 newDecoColor);
		int32_t GetBrushCount() const { return int32_t(brushes.size()); }
		RendererSettings &GetRendererSettings() { return rendererSettings; }

		bool GetDrawDeco() const;
		void SetDrawDeco(bool newDrawDeco);
		bool GetSimPaused() const;
		void SetSimPaused(bool newPaused);
		EdgeMode GetEdgeMode() const;
		void SetEdgeMode(EdgeMode newEdgeMode);
		int32_t GetQueuedFrames() const;
		void SetQueuedFrames(int32_t newQueuedFrames);
		void UpdateSimUpTo(int32_t upToIndex);

		Tool *AddCustomGol(CustomGOLData gd);
		bool RemoveCustomGol(const ByteString &identifier);

		const Brush *GetBrush(BrushIndex index) const;
		const SaveInfo *GetOnlineSave() const;

		Pos2 ResolveZoom(Pos2 point) const
		{
			if (auto affected = ResolveZoomIfAffected(point))
			{
				return *affected;
			}
			return point;
		}

		void OpenConsoleAction();
		void CloseConsoleAction();

		void Init();

		bool zoomShown = false;
		ZoomMetrics zoomMetrics;
		bool CheckZoomMetrics(const ZoomMetrics &newMetrics) const;

		void BeginPaste(std::unique_ptr<GameSave> newPasteSave);
		std::optional<ByteString> EndSelect(Pos2 first, Pos2 second, SelectMode mode, bool includePressure);
		std::optional<ByteString> TakeScreenshot(bool captureUi, ScreenshotFormat screenshotFormat);

		bool infoTipsEnabled = true;
		bool debugHud = false;
		bool drawBrush = true;
		MenuSectionIndex activeMenuSection = 0;
		std::function<void (String)> visualLogSink;
		bool threadedRendering = true;
		bool wantTextInputOverride = false;
		bool hud = true;
		uint32_t debugFlags = 0;

		static std::optional<CustomGOLData> CheckCustomGolToAdd(String ruleString, String nameString, RGB color1, RGB color2);
	};
}
