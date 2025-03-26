#include "Game.hpp"
#include "ColorPicker.hpp"
#include "ConfirmQuit.hpp"
#include "Console.hpp"
#include "Update.hpp"
#include "Common/Defer.hpp"
#include "Common/Div.hpp"
#include "Common/Log.hpp"
#include "IntroText.hpp"
#include "Gui/SdlAssert.hpp"
#include "Gui/StaticTexture.hpp"
#include "Simulation/RenderThumbnail.hpp"
#include "client/Client.h"
#include "client/GameSave.h"
#include "client/SaveFile.h"
#include "client/SaveInfo.h"
#include "graphics/Renderer.h"
#include "simulation/Air.h"
#include "simulation/Simulation.h"
#include "simulation/SimulationData.h"
#include "simulation/ElementClasses.h"
#include "gui/game/Brush.h"
#include "gui/game/tool/Tool.h"
#include "simulation/Snapshot.h"
#include "client/http/ExecVoteRequest.h"
#include "common/clipboard/Clipboard.h"
#include "common/platform/Platform.h"
#include "lua/CommandInterface.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr int32_t initialIntroTextAlpha = 10500;
		constexpr int32_t maxLogEntries         =    20;

		Game::Rect GetSaneSaveRect(Game::Pos2 point1, Game::Pos2 point2)
		{
			point1 = point1.Clamp(RES.OriginRect());
			point2 = point2.Clamp(RES.OriginRect());
			auto tlx = std::min(point1.X, point2.X);
			auto tly = std::min(point1.Y, point2.Y);
			auto brx = std::max(point1.X, point2.X);
			auto bry = std::max(point1.Y, point2.Y);
			return RectBetween(Game::Pos2{ tlx, tly }, Game::Pos2{ brx, bry });
		}
	}

	Game::Game(Gui::Host &newHost, ThreadPool &newThreadPool) :
		View(newHost),
		threadPool(newThreadPool),
		lastScreenshotTime(time(nullptr)),
		toolTipAlpha{ GetHost(), Gui::TargetAnimation<int32_t>::LinearProfile{ 400, 200 } },
		infoTipAlpha{ GetHost(), Gui::TargetAnimation<int32_t>::LinearProfile{ 400, 200 } },
		introTextAlpha{ GetHost(), Gui::TargetAnimation<int32_t>::LinearProfile{ 300 } }
	{
		introText = GetIntroText();
		decoSlots = {
			0xFFFFFFFF_argb,
			0xFF00FFFF_argb,
			0xFFFF00FF_argb,
			0xFFFFFF00_argb,
			0xFFFF0000_argb,
			0xFF00FF00_argb,
			0xFF0000FF_argb,
			0xFF000000_argb,
		};
		extraToolTextures[int32_t(ExtraToolTexture::decoSlotBackground)].data = ColorPicker::GetTransparencyPattern(Game::toolTextureDataSize);
		defaultEdgeMode = EDGE_VOID;
		defaultAmbientAirTemp = R_TEMP + 273.15f;
		activeMenuSection = SC_POWDERS;
		activeMenuSectionBeforeDeco = SC_POWDERS;
		rendererRemote = std::make_unique<Renderer>();
		renderer = rendererRemote.get();
		renderer->ClearAccumulation();
		simulation = std::make_unique<::Simulation>();
		commandInterface = CommandInterface::Create(*this); // TODO-REDO_UI: figure out when this needs to be created
		InitActions();
		InitBrushes();
		SetBrushIndex(BRUSH_CIRCLE);
		InitTools();
		introTextAlpha.SetValue(initialIntroTextAlpha);
	}

	Game::~Game()
	{
		StopRendererThread();
	}

	void Game::Init()
	{
		commandInterface->Init();
	}

	bool Game::GetRendererThreadEnabled() const
	{
		return threadedRendering;
	}

	void Game::RenderSimulation(const RenderableSimulation &sim, bool handleEvents)
	{
		rendererRemote->sim = &sim;
		rendererRemote->Clear();
		rendererRemote->RenderBackground();
		if (handleEvents)
		{
			BeforeSimDraw();
		}
		{
			// we may write graphicscache here
			auto &sd = SimulationData::Ref();
			std::unique_lock lk(sd.elementGraphicsMx);
			rendererRemote->RenderSimulation();
		}
		if (handleEvents)
		{
			AfterSimDraw();
		}
		rendererRemote->sim = nullptr;
	}

	bool Game::GetDrawDeco() const
	{
		return rendererSettings.decorationLevel != RendererSettings::decorationDisabled;
	}

	void Game::SetDrawDeco(bool newDrawDeco)
	{
		rendererSettings.decorationLevel = newDrawDeco ? RendererSettings::decorationEnabled : RendererSettings::decorationDisabled;
	}

	bool Game::GetDrawGravity() const
	{
		return rendererSettings.gravityFieldEnabled;
	}

	void Game::SetDrawGravity(bool newDrawGravity)
	{
		rendererSettings.gravityFieldEnabled = newDrawGravity;
	}

	void Game::BeforeSimDraw()
	{
		// TODO-REDO_UI
	}

	void Game::AfterSimDraw()
	{
		// TODO-REDO_UI
	}

	void Game::RendererThread()
	{
		while (true)
		{
			{
				std::unique_lock lk(rendererThreadMx);
				rendererThreadOwnsRenderer = false;
				rendererThreadCv.notify_one();
				rendererThreadCv.wait(lk, [this]() {
					return rendererThreadState == RendererThreadState::stopping || rendererThreadOwnsRenderer;
				});
				if (rendererThreadState == RendererThreadState::stopping)
				{
					break;
				}
			}
			RenderSimulation(*rendererThreadSim, false);
		}
	}

	void Game::StartRendererThread()
	{
		bool start = false;
		bool notify = false;
		{
			std::lock_guard lk(rendererThreadMx);
			if (rendererThreadState == RendererThreadState::absent)
			{
				rendererThreadSim = std::make_unique<RenderableSimulation>();
				rendererThreadResult = std::make_unique<RendererFrame>();
				rendererThreadState = RendererThreadState::running;
				start = true;
			}
			else if (rendererThreadState == RendererThreadState::paused)
			{
				rendererThreadState = RendererThreadState::running;
				notify = true;
			}
		}
		if (start)
		{
			rendererThread = std::thread([this]() {
				RendererThread();
			});
			notify = true;
		}
		if (notify)
		{
			DispatchRendererThread();
		}
	}

	void Game::StopRendererThread()
	{
		bool join = false;
		{
			std::lock_guard lk(rendererThreadMx);
			if (rendererThreadState != RendererThreadState::absent)
			{
				rendererThreadState = RendererThreadState::stopping;
				join = true;
			}
		}
		if (join)
		{
			rendererThreadCv.notify_one();
			rendererThread.join();
		}
	}

	void Game::PauseRendererThread()
	{
		bool notify = false;
		{
			std::unique_lock lk(rendererThreadMx);
			if (rendererThreadState == RendererThreadState::running)
			{
				rendererThreadState = RendererThreadState::paused;
				notify = true;
			}
		}
		if (notify)
		{
			rendererThreadCv.notify_one();
		}
		WaitForRendererThread();
	}

	void Game::DispatchRendererThread()
	{
		renderer->ApplySettings(rendererSettings);
		renderer = nullptr;
		*rendererThreadSim = *simulation;
		rendererThreadSim->useLuaCallbacks = false;
		rendererThreadOwnsRenderer = true;
		{
			std::lock_guard lk(rendererThreadMx);
			rendererThreadOwnsRenderer = true;
		}
		rendererThreadCv.notify_one();
	}

	void Game::WaitForRendererThread()
	{
		std::unique_lock lk(rendererThreadMx);
		rendererThreadCv.wait(lk, [this]() {
			return !rendererThreadOwnsRenderer;
		});
		renderer = rendererRemote.get();
	}

	void Game::HandleTick()
	{
		if (execVoteRequest && execVoteRequest->CheckDone())
		{
			try
			{
				execVoteRequest->Finish();
				std::get<OnlineSave>(*save)->SetVote(execVoteRequest->Direction());
			}
			catch (const http::RequestError &ex)
			{
				// TODO-REDO_UI: notification
			}
			execVoteRequest.reset();
		}
		if (placeZoomContext->active)
		{
			zoomMetrics = MakeZoomMetrics(zoomMetrics.from.size);
		}
		ResetDrawStateIfZoomChanged();
		if (drawState)
		{
			if (!drawState->mouseMovedSinceLastTick)
			{
				DragBrush(true);
			}
			drawState->mouseMovedSinceLastTick = false;
		}
		if (IsSimRunning())
		{
			UpdateSimUpTo(NPART);
		}
		else
		{
			BeforeSim();
		}
		auto &client = Client::Ref();
		if (!client.GetServerNotifications().empty())
		{
			for (auto &item : client.TakeServerNotifications())
			{
				AddServerNotification(item);
			}
		}
		if (client.GetUpdateInfo())
		{
			auto updateInfo = *client.TakeUpdateInfo();
			auto addNotification = [this, updateInfo](std::string wording) {
				AddNotification({
					wording,
					[this, updateInfo]() {
						Update::PushUpdateConfirm(*this, updateInfo);
					},
				});
			};
			switch (updateInfo.channel)
			{
			case UpdateInfo::channelSnapshot:
				if constexpr (MOD)
				{
					addNotification("A new mod update is available; click here to update"); // TODO-REDO_UI-TRANSLATE
				}
				else
				{
					addNotification("A new snapshot is available; click here to update"); // TODO-REDO_UI-TRANSLATE
				}
				break;

			case UpdateInfo::channelStable:
				addNotification("A new version is available; click here to update"); // TODO-REDO_UI-TRANSLATE
				break;

			case UpdateInfo::channelBeta:
				addNotification("A new beta is available; click here to update"); // TODO-REDO_UI-TRANSLATE
				break;
			}
		}
		if (pasteSave && pasteSave->thumbnailFuture.valid() && pasteSave->thumbnailFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			pasteSave->thumbnail = pasteSave->thumbnailFuture.get();
		}
	}

	void Game::HandleBeforeGui()
	{
		DrawSim();
	}

	void Game::HandleAfterGui()
	{
		while (!logEntries.empty() && !logEntries.front().alpha.GetValue())
		{
			logEntries.pop_front();
		}
		if (toolTipQueued)
		{
			toolTipAlpha.SetTarget(400);
			toolTipQueued = false;
		}
		else
		{
			toolTipAlpha.SetTarget(0);
		}
		if (!toolTipAlpha.GetValue())
		{
			toolTipInfo.reset();
		}
		if (!infoTipAlpha.GetValue())
		{
			infoTipInfo.reset();
		}
	}

	void Game::DrawSim()
	{
		if (!simTexture || !rendererFrame)
		{
			return;
		}
		drawSimFrame.video = *rendererFrame;
		DrawZoomRect();
		DrawSelect();
		if (!placeZoomContext->active &&
		    !selectContext->active &&
		    !pasteContext->active &&
		    !lastHoveredSign &&
		    drawBrush)
		{
			DrawBrush();
		}
		if (auto p = GetSimMousePos(); p && pasteSave && pasteSave->thumbnail)
		{
			auto rect = RectSized(GetPlaceSavePos(*p) * CELL, pasteSave->thumbnail->Size());
			drawSimFrame.BlendImage(pasteSave->thumbnail->Data(), 0x80, rect);
			drawSimFrame.XorDottedRect(rect);
		}
		SdlAssertZero(SDL_UpdateTexture(simTexture, nullptr, drawSimFrame.video.data(), drawSimFrame.video.Size().X * sizeof(pixel)));
	}

	void Game::QuitAction()
	{
		PushAboveThis(std::make_shared<ConfirmQuit>(GetHost()));
	}

	void Game::PasteAction()
	{
		auto *clip = Clipboard::GetClipboardData();
		if (!clip)
		{
			VisualLog("Nothing on the clipboard to paste"); // TODO-REDO_UI-TRANSLATE
			return;
		}
		BeginPaste(std::make_unique<GameSave>(*clip));
	}

	void Game::CopyAction()
	{
		selectMode = SelectMode::copy;
		SetCurrentActionContext(selectContext);
	}

	void Game::CutAction()
	{
		selectMode = SelectMode::cut;
		SetCurrentActionContext(selectContext);
	}

	void Game::StampAction()
	{
		selectMode = SelectMode::stamp;
		SetCurrentActionContext(selectContext);
	}

	void Game::TakeScreenshotAction()
	{
		TakeScreenshot(false, ScreenshotFormat::png);
	}

	void Game::InstallAction()
	{
		// TODO-REDO_UI
	}

	void Game::ToggleFullscreenAction()
	{
		// TODO-REDO_UI
	}

	std::optional<ByteString> Game::TakeScreenshot(bool captureUi, ScreenshotFormat screenshotFormat)
	{
		// TODO-REDO_UI: better error handling
		// TODO-REDO_UI: use captureUi
		auto screenshot = std::make_unique<VideoBuffer>(rendererFrame->data(), RES, rendererFrame->Size().X);
		ByteString filename;
		{
			// Optional suffix to distinguish screenshots taken at the exact same time
			ByteString suffix;
			auto screenshotTime = time(nullptr);
			if (screenshotTime == lastScreenshotTime)
			{
				screenshotIndex += 1;
				suffix = ByteString::Build(" (", screenshotIndex, ")");
			}
			else
			{
				screenshotIndex = 1;
			}
			lastScreenshotTime = screenshotTime;
			std::string date = format::UnixtimeToDate(screenshotTime, "%Y-%m-%d %H.%M.%S");
			filename = ByteString::Build("screenshot ", date, suffix);
		}
		switch (screenshotFormat)
		{
		case ScreenshotFormat::bmp:
			filename += ".bmp";
			{
				// We should be able to simply use SDL_PIXELFORMAT_XRGB8888 here with a bit depth of 32 to convert RGBA data to RGB data,
				// and save the resulting surface directly. However, ubuntu-18.04 ships SDL2 so old that it doesn't have
				// SDL_PIXELFORMAT_XRGB8888, so we first create an RGBA surface and then convert it.
				auto *rgbaSurface = SdlAssertPtr(SDL_CreateRGBSurfaceWithFormatFrom(screenshot->Data(), screenshot->Size().X, screenshot->Size().Y, 32, screenshot->Size().X * sizeof(pixel), SDL_PIXELFORMAT_ARGB8888));
				Defer freeRgbaSurface([rgbaSurface]() {
					SDL_FreeSurface(rgbaSurface);
				});
				auto *rgbSurface = SdlAssertPtr(SDL_ConvertSurfaceFormat(rgbaSurface, SDL_PIXELFORMAT_RGB888, 0));
				Defer freeRgbSurface([rgbSurface]() {
					SDL_FreeSurface(rgbSurface);
				});
				if (SDL_SaveBMP(rgbSurface, filename.c_str()))
				{
					Powder::Log("SDL_SaveBMP failed: ", SDL_GetError());
					return std::nullopt;
				}
			}
			break;

		case ScreenshotFormat::ppm:
			filename += ".ppm";
			if (!Platform::WriteFile(screenshot->ToPPM(), filename))
			{
				return std::nullopt;
			}
			break;

		case ScreenshotFormat::png:
			filename += ".png";
			if (auto data = screenshot->ToPNG())
			{
				if (!Platform::WriteFile(*data, filename))
				{
					return std::nullopt;
				}
			}
			else
			{
				return std::nullopt;
			}
		}
		return filename;
	}

	void Game::LoadLastStampAction()
	{
		struct ReadFileError : public std::runtime_error // TODO: use exceptions in Client::GetStamp
		{
			using runtime_error::runtime_error;
		};
		auto &stampIDs = Client::Ref().GetStamps();
		if (stampIDs.empty())
		{
			VisualLog("No last stamp to paste"); // TODO-REDO_UI-TRANSLATE
			return;
		}
		try
		{
			auto saveFile = Client::Ref().GetStamp(stampIDs[0]);
			if (!saveFile)
			{
				throw ReadFileError("cannot access file");
			}
			if (!saveFile->LazyGetGameSave())
			{
				throw ReadFileError(saveFile->GetError().ToUtf8());
			}
			BeginPaste(saveFile->TakeGameSave());
		}
		catch (const ReadFileError &ex)
		{
			// TODO-REDO_UI: error popup instead?
			VisualLog(ByteString("Failed to load stamp: ") + ex.what()); // TODO-REDO_UI-TRANSLATE
		}
	}

	void Game::ApplyPasteTransform()
	{
		auto remX = floorDiv(pasteSave->translate.X, CELL).second;
		auto remY = floorDiv(pasteSave->translate.Y, CELL).second;
		pasteSave->transformed = std::make_unique<GameSave>(*pasteSave->original);
		pasteSave->transformed->Transform(pasteSave->transform, { remX, remY });
		pasteSave->thumbnail.reset(); // TODO-REDO_UI: approximate new thumbnail
		pasteSave->thumbnailFuture = Simulation::RenderThumbnail(
			threadPool,
			std::make_unique<GameSave>(*pasteSave->transformed),
			pasteSave->transformed->blockSize * CELL,
			rendererSettings,
			true
		);
	}

	void Game::TranslatePasteSave(Pos2 addToTranslate)
	{
		pasteSave->translate += addToTranslate;
		ApplyPasteTransform();
	}

	void Game::TransformPasteSave(Mat2x2 mulToTransform)
	{
		pasteSave->translate = Pos2::Zero; // reset offset
		pasteSave->transform = mulToTransform * pasteSave->transform;
		ApplyPasteTransform();
	}

	void Game::BeginPaste(std::unique_ptr<GameSave> newPasteSave)
	{
		if (newPasteSave->missingElements)
		{
			// TODO-REDO_UI: display like the help text for selectContext
			VisualLog("Paste content has missing custom elements"); // TODO-REDO_UI-TRANSLATE
		}
		pasteSave = PasteSave{};
		pasteSave->original = std::move(newPasteSave);
		ApplyPasteTransform();
		SetCurrentActionContext(pasteContext);
	}

	Game::Pos2 Game::GetPlaceSavePos(Game::Pos2 pos) const
	{
		auto [ trQuoX, trRemX ] = floorDiv(pasteSave->translate.X, CELL);
		auto [ trQuoY, trRemY ] = floorDiv(pasteSave->translate.Y, CELL);
		auto usefulSize = pasteSave->transformed->blockSize * CELL;
		if (trRemX) usefulSize.X -= CELL;
		if (trRemY) usefulSize.Y -= CELL;
		auto cursorCell = (usefulSize - Pos2{ CELL, CELL }) / 2 - Pos2{ trQuoX, trQuoY } * CELL; // stamp coordinates
		auto unaligned = pos - cursorCell;
		auto quoX = floorDiv(unaligned.X, CELL).first;
		auto quoY = floorDiv(unaligned.Y, CELL).first;
		return { quoX, quoY };
	}

	void Game::EndPaste(Pos2 pos, bool includePressure)
	{
		CreateHistoryEntry();
		simulation->Load(pasteSave->transformed.get(), includePressure, GetPlaceSavePos(pos));
		SetSimPaused(pasteSave->transformed->paused || GetSimPaused());
		// Client::Ref().MergeStampAuthorInfo(pasteSave->transformed->authors); // TODO-REDO_UI
	}

	void Game::EndSelectAction(Pos2 first, Pos2 second, SelectMode mode, bool includePressure)
	{
		auto stampName = EndSelect(first, second, mode, includePressure);
		if (mode == SelectMode::stamp && !stampName)
		{
			// TODO-REDO_UI: better error handling
			PushMessage("Could not create stamp", "Error serializing save file", true, nullptr); // TODO-REDO_UI-TRANSLATE
		}
	}

	std::optional<ByteString> Game::EndSelect(Pos2 first, Pos2 second, SelectMode mode, bool includePressure)
	{
		auto rect = GetSaneSaveRect(first, second);
		auto newSave = simulation->Save(includePressure, rect);
		newSave->paused = GetSimPaused();
		switch (mode)
		{
		case SelectMode::copy:
		case SelectMode::cut:
			{
				Bson clipboardInfo;
				clipboardInfo["type"] = "clipboard";
				auto user = Client::Ref().GetAuthUser();
				clipboardInfo["username"] = user ? user->Username : ByteString("");
				clipboardInfo["date"] = int64_t(time(nullptr));
				Client::Ref().SaveAuthorInfo(clipboardInfo);
				newSave->authors = clipboardInfo;
				Clipboard::SetClipboardData(std::move(newSave));
			}
			if (mode == SelectMode::cut)
			{
				CreateHistoryEntry();
				simulation->clear_area(rect.pos.X, rect.pos.Y, rect.size.X, rect.size.Y);
			}
			break;

		case SelectMode::stamp:
			if (auto stampName = Client::Ref().AddStamp(std::move(newSave)); !stampName.empty())
			{
				return stampName;
			}
			break;
		}
		return std::nullopt;
	}

	void Game::OpenConsoleAction()
	{
		if (!console)
		{
			console = std::make_shared<Console>(*this);
		}
		PushAboveThis(console);
		console->Open();
	}

	void Game::CloseConsoleAction()
	{
		if (!console)
		{
			return;
		}
		console->Cancel();
	}

	bool Game::Cancel()
	{
		if (showRenderer)
		{
			showRenderer = false;
			return true;
		}
		if (GetShowingDecoTools())
		{
			activeMenuSection = activeMenuSectionBeforeDeco;
			return true;
		}
		QuitAction();
		return true;
	}

	void Game::DropFile(ByteString file)
	{
		if (!(file.EndsWith(".cps") || file.EndsWith(".stm")))
		{
			PushMessage("Error loading save", "Dropped file is not a TPT save file (.cps or .stm format)", true, nullptr); // TODO-REDO_UI-TRANSLATE
			return;
		}
		if (file.EndsWith(".stm"))
		{
			auto saveFile = Client::Ref().GetStamp(file);
			if (!saveFile || !saveFile->GetGameSave())
			{
				PushMessage("Error loading stamp", "Dropped stamp could not be loaded: " + saveFile->GetError().ToUtf8(), true, nullptr); // TODO-REDO_UI-TRANSLATE
				return;
			}
			BeginPaste(saveFile->TakeGameSave());
			return;
		}
		auto saveFile = Client::Ref().LoadSaveFile(file);
		if (!saveFile)
		{
			// TODO-REDO_UI: better error handling
			return;
		}
		if (saveFile->GetError().length())
		{
			PushMessage("Error loading save", "Dropped save file could not be loaded: " + saveFile->GetError().ToUtf8(), true, nullptr); // TODO-REDO_UI-TRANSLATE
			return;
		}
		SetSave(std::move(saveFile), GetIncludePressure());
	}

	bool Game::HandleEvent(const SDL_Event &event)
	{
		switch (event.type)
		{
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEWHEEL:
		case SDL_KEYDOWN:
			if (event.type == SDL_KEYDOWN && event.key.repeat)
			{
				break;
			}
			DismissIntroText();
			break;
		}
#if DebugGuiView
		if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.scancode == SDL_SCANCODE_KP_MULTIPLY)
		{
			shouldDoDebugDump = true;
		}
#endif
		auto handledByView = View::HandleEvent(event);
		if (MayBeHandledExclusively(event) && handledByView && !lastHoveredTool)
		{
			return true;
		}
		auto handledByInputMapper = InputMapper::HandleEvent(event);
		if (MayBeHandledExclusively(event) && handledByInputMapper)
		{
			return true;
		}
		switch (event.type)
		{
		case SDL_MOUSEMOTION:
			ResetDrawStateIfZoomChanged();
			if (drawState)
			{
				if (DragBrush(false))
				{
					drawState->mouseMovedSinceLastTick = true;
				}
			}
			break;

		case SDL_DROPFILE:
			DropFile(event.drop.file);
			DismissIntroText();
			break;
		}
		return false;
	}

	::Simulation &Game::GetSimulation()
	{
		return *simulation.get();
	}

	Renderer &Game::GetRenderer()
	{
		PauseRendererThread();
		return *renderer;
	}

	void Game::ToggleSandEffectAction()
	{
		SetSandEffect(!GetSandEffect());
	}

	void Game::ToggleDrawGravityAction()
	{
		SetDrawGravity(!GetDrawGravity());
	}

	void Game::ToggleDrawDecoAction()
	{
		SetDrawDeco(!GetDrawDeco());
	}

	void Game::ToggleNewtonianGravityAction()
	{
		SetNewtonianGravity(!GetNewtonianGravity());
	}

	void Game::ToggleAmbientHeatAction()
	{
		SetAmbientHeat(!GetAmbientHeat());
	}

	void Game::ToggleSimPausedAction()
	{
		SetSimPaused(!GetSimPaused());
	}

	void Game::ToggleDebugHudAction()
	{
		debugHud = !debugHud;
	}

	void Game::ToggleFindAction()
	{
		auto fp = GetFindParameters();
		if (rendererSettings.findingElement == fp)
		{
			rendererSettings.findingElement = std::nullopt;
		}
		else
		{
			rendererSettings.findingElement = fp;
		}
	}

	void Game::VisualLog(std::string message)
	{
		if (visualLogSink)
		{
			visualLogSink(ByteString(message).FromUtf8());
			return;
		}
		logEntries.push_back(LogEntry{ { GetHost(), Gui::TargetAnimation<int32_t>::LinearProfile{ 170 }, 0, 600 }, std::move(message) });
		while (int32_t(logEntries.size()) > maxLogEntries)
		{
			logEntries.pop_front();
		}
	}

	void Game::Log(std::string message)
	{
		VisualLog(message);
		Powder::Log(message);
	}

	void Game::LoadGameSave(const GameSave &gameSave, bool includePressure)
	{
		ApplySaveParametersToSim(gameSave);
		auto *sim = simulation.get();
		sim->clear_sim();
		PauseRendererThread();
		renderer->ClearAccumulation();
		sim->Load(&gameSave, includePressure, { 0, 0 });
	}

	void Game::ReloadSimAction()
	{
		if (!ReloadSim(GetIncludePressure()))
		{
			VisualLog("No save to reload from"); // TODO-REDO_UI-TRANSLATE
		}
	}

	void Game::ClearSim()
	{
		CreateHistoryEntry();
		SetSave(std::nullopt, false);
	}

	void Game::SetSaveInternal(std::optional<Save> newSave, bool includePressure, bool resetExecVote)
	{
		if (resetExecVote && execVoteRequest)
		{
			execVoteRequest.reset(); // TODO-REDO_UI: notification
		}
		DismissIntroText();
		save = std::move(newSave);
		if (save)
		{
			std::visit([this, includePressure](const auto &save) {
				const auto &gameSave = *save->GetGameSave();
				LoadGameSave(gameSave, includePressure);
				auto newAuthors = gameSave.authors;
				if constexpr (std::is_same_v<std::remove_cvref_t<decltype(save)>, OnlineSave>)
				{
					// This save was created before logging existed
					// Add in the correct info
					if (newAuthors.IsEmpty())
					{
						newAuthors["type"]        = "save";
						newAuthors["id"]          = save->GetID();
						newAuthors["username"]    = save->GetUserName();
						newAuthors["title"]       = save->GetName().ToUtf8();
						newAuthors["description"] = save->GetDescription().ToUtf8();
						newAuthors["published"]   = int32_t(save->GetPublished());
						newAuthors["date"]        = int64_t(save->updatedDate);
					}
					// This save was probably just created, and we didn't know the ID when creating it
					// Update with the proper ID
					else if (newAuthors.Get("id", -1) == 0 || newAuthors.Get("id", -1) == -1)
					{
						newAuthors["id"] = save->GetID();
					}
				}
				authors = newAuthors;
			}, *save);
		}
		else
		{
			auto *sim = simulation.get();
			sim->gravityMode = GRAV_VERTICAL;
			sim->customGravityX = 0.0f;
			sim->customGravityY = 0.0f;
			sim->air->airMode = AIR_ON;
			sim->legacy_enable = false;
			sim->water_equal_test = false;
			sim->SetEdgeMode(defaultEdgeMode);
			sim->air->ambientAirTemp = defaultAmbientAirTemp;
			sim->clear_sim();
			PauseRendererThread();
			renderer->ClearAccumulation();
			authors = Bson{};
		}
	}

	void Game::SetSave(std::optional<Save> newSave, bool includePressure)
	{
		SetSaveInternal(std::move(newSave), includePressure, true);
	}

	bool Game::ReloadSim(bool includePressure)
	{
		if (!save)
		{
			return false;
		}
		CreateHistoryEntry();
		std::optional<Save> newSave;
		std::swap(save, newSave);
		SetSaveInternal(std::move(newSave), includePressure, false);
		return true;
	}

	void Game::ApplySaveParametersToSim(const GameSave &save)
	{
		auto *sim = simulation.get();
		SetSimPaused(save.paused | GetSimPaused());
		sim->gravityMode         = save.gravityMode;
		sim->customGravityX      = save.customGravityX;
		sim->customGravityY      = save.customGravityY;
		sim->air->airMode        = save.airMode;
		sim->air->ambientAirTemp = save.ambientAirTemp;
		sim->edgeMode            = save.edgeMode;
		sim->legacy_enable       = save.legacyEnable;
		sim->water_equal_test    = save.waterEEnabled;
		sim->aheat_enable        = save.aheatEnable;
		sim->frameCount          = save.frameCount;
		sim->ensureDeterminism   = save.ensureDeterminism;
		sim->EnableNewtonianGravity(save.gravityEnable);
		if (save.hasRngState)
		{
			sim->rng.state(save.rngState);
		}
		else
		{
			sim->rng = RNG();
		}
	}

	bool Game::GetSimPaused() const
	{
		return simPaused;
	}

	void Game::SetSimPaused(bool newPaused)
	{
		auto *sim = simulation.get();
		if (!newPaused && sim->debug_nextToUpdate > 0)
		{
			auto message = ByteString::Build("Updated particles from #", sim->debug_nextToUpdate, " to end due to unpause");
			UpdateSimUpTo(NPART);
			VisualLog(message);
		}
		simPaused = newPaused;
	}

	bool Game::GetSandEffect() const
	{
		return simulation->pretty_powder;
	}

	void Game::SetSandEffect(bool newSandEffect)
	{
		simulation->pretty_powder = newSandEffect;
	}

	bool Game::GetNewtonianGravity() const
	{
		return bool(simulation->grav);
	}

	void Game::SetNewtonianGravity(bool newNewtonianGravity)
	{
		simulation->EnableNewtonianGravity(newNewtonianGravity);
	}

	bool Game::GetAmbientHeat() const
	{
		return simulation->aheat_enable;
	}

	void Game::SetAmbientHeat(bool newAmbientHeat)
	{
		simulation->aheat_enable = newAmbientHeat;
	}

	bool Game::GetVorticityCoeff() const
	{
		return simulation->air->vorticityCoeff;
	}

	void Game::SetVorticityCoeff(bool newVorticityCoeff)
	{
		simulation->air->vorticityCoeff = newVorticityCoeff;
	}

	bool Game::GetHeat() const
	{
		return simulation->legacy_enable;
	}

	void Game::SetHeat(bool newHeat)
	{
		simulation->legacy_enable = newHeat;
	}

	AirMode Game::GetAirMode() const
	{
		return AirMode(simulation->air->airMode);
	}

	void Game::SetAirMode(AirMode newAirMode)
	{
		simulation->air->airMode = int32_t(newAirMode);
	}

	GravityMode Game::GetGravityMode() const
	{
		return GravityMode(simulation->gravityMode);
	}

	void Game::SetGravityMode(GravityMode newGravityMode)
	{
		simulation->gravityMode = int32_t(newGravityMode);
	}

	EdgeMode Game::GetEdgeMode() const
	{
		return EdgeMode(simulation->edgeMode);
	}

	void Game::SetEdgeMode(EdgeMode newEdgeMode)
	{
		simulation->SetEdgeMode(int32_t(newEdgeMode));
	}

	float Game::GetAmbientAirTemp() const
	{
		return simulation->air->ambientAirTemp;
	}

	void Game::SetAmbientAirTemp(float newAmbientAirTemp)
	{
		simulation->air->ambientAirTemp = newAmbientAirTemp;
	}

	bool Game::GetWaterEqualization() const
	{
		return simulation->water_equal_test;
	}

	void Game::SetWaterEqualization(bool newWaterEqualization)
	{
		simulation->water_equal_test = newWaterEqualization;
	}

	bool Game::IsSimRunning() const
	{
		return !simPaused || queuedFrames;
	}

	void Game::BeforeSim()
	{
		auto willUpdate = IsSimRunning();
		auto *sim = simulation.get();
		if (willUpdate)
		{
			// TODO-REDO_UI: call lua
		}
		sim->BeforeSim(willUpdate);
	}

	void Game::AfterSim()
	{
		simulation->AfterSim();
		// TODO-REDO_UI: call lua
	}

	void Game::UpdateSimUpTo(int32_t upToIndex)
	{
		auto *sim = simulation.get();
		if (upToIndex < sim->debug_nextToUpdate)
		{
			upToIndex = NPART;
		}
		if (sim->debug_nextToUpdate == 0)
		{
			BeforeSim();
		}
		sim->UpdateParticles(sim->debug_nextToUpdate, upToIndex);
		if (queuedFrames)
		{
			queuedFrames--;
		}
		if (upToIndex < NPART)
		{
			sim->debug_nextToUpdate = upToIndex;
		}
		else
		{
			AfterSim();
			sim->debug_nextToUpdate = 0;
		}
	}

	void Game::CycleEdgeModeAction()
	{
		auto *sim = simulation.get();
		sim->SetEdgeMode(EdgeMode((int32_t(simulation->edgeMode) + 1) % int32_t(NUM_EDGEMODES)));
		defaultEdgeMode = EdgeMode(sim->edgeMode);
		switch (sim->edgeMode)
		{
		case EDGE_VOID : QueueInfoTip("Edge Mode: Void" ); break; // TODO-REDO_UI-TRANSLATE
		case EDGE_SOLID: QueueInfoTip("Edge Mode: Solid"); break; // TODO-REDO_UI-TRANSLATE
		case EDGE_LOOP : QueueInfoTip("Edge Mode: Loop" ); break; // TODO-REDO_UI-TRANSLATE
		}
	}

	void Game::CycleAirModeAction()
	{
		auto *sim = simulation.get();
		simulation->air->airMode = AirMode((int32_t(simulation->air->airMode) + 1) % int32_t(NUM_AIRMODES));
		switch (sim->air->airMode)
		{
		case AIR_ON         : QueueInfoTip("Air: On"          ); break; // TODO-REDO_UI-TRANSLATE
		case AIR_PRESSUREOFF: QueueInfoTip("Air: Pressure Off"); break; // TODO-REDO_UI-TRANSLATE
		case AIR_VELOCITYOFF: QueueInfoTip("Air: Velocity Off"); break; // TODO-REDO_UI-TRANSLATE
		case AIR_OFF        : QueueInfoTip("Air: Off"         ); break; // TODO-REDO_UI-TRANSLATE
		case AIR_NOUPDATE   : QueueInfoTip("Air: No Update"   ); break; // TODO-REDO_UI-TRANSLATE
		}
	}

	void Game::SetOnTop(bool newOnTop)
	{
		if (!newOnTop)
		{
			EndAllInputs();
			PauseRendererThread();
		}
	}

	void Game::DoSimFrameStep()
	{
		queuedFrames += 1;
		SetSimPaused(true);
	}

	int32_t Game::GetQueuedFrames() const
	{
		return queuedFrames;
	}

	void Game::SetQueuedFrames(int32_t newQueuedFrames)
	{
		queuedFrames = newQueuedFrames;
	}

	void Game::GrowGridAction()
	{
		rendererSettings.gridSize = FloorMod(rendererSettings.gridSize + 1, 10);
	}

	void Game::ShrinkGridAction()
	{
		rendererSettings.gridSize = FloorMod(rendererSettings.gridSize - 1, 10);
	}

	void Game::ToggleGravityGridAction()
	{
		rendererSettings.gravityFieldEnabled = !rendererSettings.gravityFieldEnabled;
	}

	void Game::ToggleIntroTextAction()
	{
		introTextAlpha.SetValue(introTextAlpha.GetValue() ? 0 : initialIntroTextAlpha);
	}

	void Game::ToggleHudAction()
	{
		hud = !hud;
	}

	void Game::ToggleDecoAction()
	{
		if (rendererSettings.decorationLevel == RendererSettings::decorationEnabled)
		{
			rendererSettings.decorationLevel = RendererSettings::decorationDisabled;
			QueueInfoTip("Decorations Layer: Off"); // TODO-REDO_UI-TRANSLATE
		}
		else
		{
			rendererSettings.decorationLevel = RendererSettings::decorationEnabled;
			QueueInfoTip("Decorations Layer: On"); // TODO-REDO_UI-TRANSLATE
		}
	}

	void Game::ToggleDecoMenuAction()
	{
		if (GetShowingDecoTools())
		{
			activeMenuSection = activeMenuSectionBeforeDeco;
		}
		else
		{
			activeMenuSection = SC_DECO;
			rendererSettings.decorationLevel = RendererSettings::decorationEnabled;
			SetSimPaused(true);
		}
	}

	void Game::ResetAmbientHeatAction()
	{
		simulation->air->ClearAirH();
	}

	void Game::ResetAirAction()
	{
		auto *sim = simulation.get();
		sim->air->Clear();
		for (int32_t i = 0; i < NPART; ++i)
		{
			if (GameSave::PressureInTmp3(sim->parts[i].type))
			{
				sim->parts[i].tmp3 = 0;
			}
		}
	}

	void Game::ResetSparkAction()
	{
		auto &sd = SimulationData::CRef();
		auto *sim = simulation.get();
		for (int32_t i = 0; i < NPART; ++i)
		{
			if (sim->parts[i].type == PT_SPRK)
			{
				if (sim->parts[i].ctype >= 0 && sim->parts[i].ctype < PT_NUM && sd.elements[sim->parts[i].ctype].Enabled)
				{
					sim->parts[i].type = sim->parts[i].ctype;
					sim->parts[i].ctype = sim->parts[i].life = 0;
				}
				else
				{
					sim->kill_part(i);
				}
			}
		}
		std::memset(sim->wireless, 0, sizeof(sim->wireless));
	}

	void Game::InvertAirAction()
	{
		simulation->air->Invert();
	}

	void Game::ToggleReplaceModeFlag(int32_t flag)
	{
		auto prev = bool(simulation->replaceModeFlags & flag);
		simulation->replaceModeFlags &= ~(REPLACE_MODE | SPECIFIC_DELETE);
		if (!prev)
		{
			simulation->replaceModeFlags |= flag;
		}
	}

	void Game::ToggleReplaceAction()
	{
		ToggleReplaceModeFlag(REPLACE_MODE);
	}

	void Game::ToggleSdeleteAction()
	{
		ToggleReplaceModeFlag(SPECIFIC_DELETE);
	}

	bool Game::GetShowingDecoTools() const
	{
		return activeMenuSection == SC_DECO;
	}

	std::optional<Rgb8> Game::GetColorUnderMouse() const
	{
		if (auto p = GetSimMousePos())
		{
			return Rgb8::Unpack((*rendererFrame)[*p]);
		}
		return std::nullopt;
	}

	void Game::UseRendererPreset(int32_t index)
	{
		auto &preset = Renderer::renderModePresets[index];
		rendererSettings.renderMode        = preset.renderMode;
		rendererSettings.displayMode       = preset.displayMode;
		rendererSettings.colorMode         = preset.colorMode;
		rendererSettings.wantHdispLimitMin = preset.wantHdispLimitMin;
		rendererSettings.wantHdispLimitMax = preset.wantHdispLimitMax;
	}

	void Game::Exit()
	{
		if (execVoteRequest)
		{
			execVoteRequest.reset(); // TODO-REDO_UI: give to something else
		}
		View::Exit();
	}

	void Game::AddNotification(NotificationEntry notificationEntry)
	{
		notificationEntries.push_back(notificationEntry);
	}

	void Game::AddServerNotification(ServerNotification serverNotification)
	{
		AddNotification({
			serverNotification.text.ToUtf8(),
			[link = serverNotification.link]() {
				Platform::OpenURI(link);
			},
		});
	}

	bool Game::GetIncludePressure() const
	{
		// TODO-REDO_UI: make sure invertIncludePressure survives across SetOnTop changes
		return true ^ invertIncludePressure; // TODO-REDO_UI: invert from config
	}

	const SaveInfo *Game::GetOnlineSave() const
	{
		return (save && std::holds_alternative<OnlineSave>(*save)) ? std::get<OnlineSave>(*save).get() : nullptr;
	}

	bool Game::GetWantTextInput() const
	{
		return View::GetWantTextInput() || wantTextInputOverride;
	}

	std::optional<Game::Pos2> Game::GetSimMousePos() const
	{
		if (auto m = GetMousePos())
		{
			auto p = ResolveZoom(*m);
			if (RES.OriginRect().Contains(p))
			{
				return p;
			}
		}
		return std::nullopt;
	}

	void Game::SetDecoColor(Rgba8 newDecoColor)
	{
		decoColor = newDecoColor;
		shouldUpdateDecoTools = true;
	}
}
