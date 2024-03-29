#include "PowderToySDL.h"
#include "SimulationConfig.h"
#include "WindowIcon.h"
#include "Config.h"
#include "gui/interface/Engine.h"
#include "graphics/Graphics.h"
#include "common/platform/Platform.h"
#include "common/Clipboard.h"
#include <iostream>

int desktopWidth = 1280;
int desktopHeight = 1024;
SDL_Window *sdl_window = NULL;
SDL_Renderer *sdl_renderer = NULL;
SDL_Texture *sdl_texture = NULL;
bool vsyncHint = false;
WindowFrameOps currentFrameOps;
bool momentumScroll = true;
bool showAvatars = true;
uint64_t lastTick = 0;
uint64_t lastFpsUpdate = 0;
bool showLargeScreenDialog = false;
int mousex = 0;
int mousey = 0;
int mouseButton = 0;
bool mouseDown = false;
bool calculatedInitialMouse = false;
bool hasMouseMoved = false;
double correctedFrameTimeAvg = 0;
uint64_t drawingTimer = 0;
uint64_t frameStart = 0;
uint64_t oldFrameStart = 0;

void StartTextInput()
{
	SDL_StartTextInput();
}

void StopTextInput()
{
	SDL_StopTextInput();
}

void SetTextInputRect(int x, int y, int w, int h)
{
	// Why does SDL_SetTextInputRect not take logical coordinates???
	SDL_Rect rect;
	float wx, wy, wwx, why;
	SDL_RenderCoordinatesToWindow(sdl_renderer, float(x), float(y), &wx, &wy);
	SDL_RenderCoordinatesToWindow(sdl_renderer, float(x + w), float(y + h), &wwx, &why);
	rect.x = wx;
	rect.y = wy;
	rect.w = wwx - wx;
	rect.h = why - wy;
	SDL_SetTextInputRect(&rect);
}

void ClipboardPush(ByteString text)
{
	SDL_SetClipboardText(text.c_str());
}

ByteString ClipboardPull()
{
	return ByteString(SDL_GetClipboardText());
}

int GetModifiers()
{
	return SDL_GetModState();
}

unsigned int GetTicks()
{
	return SDL_GetTicks();
}

void blit(pixel *vid)
{
	SDL_UpdateTexture(sdl_texture, NULL, vid, WINDOWW * sizeof (Uint32));
	// need to clear the renderer if there are black edges (fullscreen, or resizable window)
	if (currentFrameOps.fullscreen || currentFrameOps.resizable)
		SDL_RenderClear(sdl_renderer);
	SDL_RenderTexture(sdl_renderer, sdl_texture, NULL, NULL);
	SDL_RenderPresent(sdl_renderer);
}

void SDLOpen()
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "Initializing SDL (video subsystem): %s\n", SDL_GetError());
		Platform::Exit(-1);
	}

	SDLSetScreen();

	int displayIndex = SDL_GetDisplayForWindow(sdl_window);
	if (displayIndex >= 0)
	{
		SDL_Rect rect;
		if (!SDL_GetDisplayUsableBounds(displayIndex, &rect))
		{
			desktopWidth = rect.w;
			desktopHeight = rect.h;
		}
	}

	StopTextInput();
}

void SDLClose()
{
	if (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_OPENGL)
	{
		// * nvidia-460 egl registers callbacks with x11 that end up being called
		//   after egl is unloaded unless we grab it here and release it after
		//   sdl closes the display. this is an nvidia driver weirdness but
		//   technically an sdl bug. glfw has this fixed:
		//   https://github.com/glfw/glfw/commit/9e6c0c747be838d1f3dc38c2924a47a42416c081
		SDL_GL_LoadLibrary(NULL);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		SDL_GL_UnloadLibrary();
	}
	SDL_Quit();
}

void SDLSetScreen()
{
	auto newFrameOps = ui::Engine::Ref().windowFrameOps;
	auto newVsyncHint = std::holds_alternative<FpsLimitVsync>(ui::Engine::Ref().GetFpsLimit());
	if (FORCE_WINDOW_FRAME_OPS == forceWindowFrameOpsEmbedded)
	{
		newFrameOps.resizable = false;
		newFrameOps.fullscreen = false;
		newFrameOps.forceIntegerScaling = false;
	}
	if (FORCE_WINDOW_FRAME_OPS == forceWindowFrameOpsHandheld)
	{
		newFrameOps.resizable = false;
		newFrameOps.fullscreen = true;
		newFrameOps.forceIntegerScaling = false;
	}

	auto currentFrameOpsNorm = currentFrameOps.Normalize();
	auto newFrameOpsNorm = newFrameOps.Normalize();
	auto recreate = !sdl_window ||
	                // Recreate the window when toggling fullscreen, due to occasional issues
	                newFrameOpsNorm.fullscreen       != currentFrameOpsNorm.fullscreen       ||
	                // Also recreate it when enabling resizable windows, to fix bugs on windows,
	                //  see https://github.com/jacob1/The-Powder-Toy/issues/24
	                newFrameOpsNorm.resizable        != currentFrameOpsNorm.resizable        ||
	                newVsyncHint != vsyncHint;

	if (!(recreate ||
	      newFrameOpsNorm.scale               != currentFrameOpsNorm.scale               ||
	      newFrameOpsNorm.forceIntegerScaling != currentFrameOpsNorm.forceIntegerScaling ||
	      newFrameOpsNorm.blurryScaling       != currentFrameOpsNorm.blurryScaling))
	{
		return;
	}

	auto size = WINDOW * newFrameOpsNorm.scale;
	if (sdl_window && newFrameOpsNorm.resizable)
	{
		SDL_GetWindowSize(sdl_window, &size.X, &size.Y);
	}

	if (recreate)
	{
		if (sdl_texture)
		{
			SDL_DestroyTexture(sdl_texture);
			sdl_texture = NULL;
		}
		if (sdl_renderer)
		{
			SDL_DestroyRenderer(sdl_renderer);
			sdl_renderer = NULL;
		}
		if (sdl_window)
		{
			SaveWindowPosition();
			SDL_DestroyWindow(sdl_window);
			sdl_window = NULL;
		}

		unsigned int flags = 0;
		if (newFrameOpsNorm.fullscreen)
		{
			flags |= SDL_WINDOW_FULLSCREEN;
		}
		if (newFrameOpsNorm.resizable)
		{
			flags |= SDL_WINDOW_RESIZABLE;
		}
		sdl_window = SDL_CreateWindow(APPNAME, size.X, size.Y, flags);
		if (!sdl_window)
		{
			fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
			Platform::Exit(-1);
		}
		if constexpr (SET_WINDOW_ICON)
		{
			WindowIcon(sdl_window);
		}
		SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
		sdl_renderer = SDL_CreateRenderer(sdl_window, NULL);
		if (!sdl_renderer)
		{
			fprintf(stderr, "SDL_CreateRenderer failed; available renderers:\n");
			int num = SDL_GetNumRenderDrivers();
			for (int i = 0; i < num; ++i)
			{
				fprintf(stderr, " - %s\n", SDL_GetRenderDriver(i));
			}
			Platform::Exit(-1);
		}
		sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WINDOWW, WINDOWH);
		if (!sdl_texture)
		{
			fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
			Platform::Exit(-1);
		}
		SDL_RaiseWindow(sdl_window);
	}
	if (SDL_SetRenderLogicalPresentation(
		sdl_renderer,
		WINDOWW,
		WINDOWH,
		newFrameOpsNorm.forceIntegerScaling ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE : SDL_LOGICAL_PRESENTATION_LETTERBOX,
		newFrameOpsNorm.blurryScaling ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST
	))
	{
		fprintf(stderr, "SDL_SetRenderLogicalPresentation failed: %s\n", SDL_GetError());
	}
	if (SDL_SetRenderVSync(sdl_renderer, vsyncHint ? 1 : 0))
	{
		fprintf(stderr, "SDL_SetRenderVSync failed: %s\n", SDL_GetError());
	}
	if (!(newFrameOpsNorm.resizable && SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_MAXIMIZED))
	{
		SDL_SetWindowSize(sdl_window, size.X, size.Y);
		LoadWindowPosition();
	}
	UpdateFpsLimit();
	if (newFrameOpsNorm.fullscreen)
	{
		SDL_RaiseWindow(sdl_window);
	}
	currentFrameOps = newFrameOps;
	vsyncHint = newVsyncHint;
}

static void EventProcess(const SDL_Event &event)
{
	auto &engine = ui::Engine::Ref();
	switch (event.type)
	{
	case SDL_EVENT_QUIT:
		if (ALLOW_QUIT && (engine.GetFastQuit() || engine.CloseWindow()))
		{
			engine.Exit();
		}
		break;
	case SDL_EVENT_KEY_DOWN:
		if (SDL_GetModState() & SDL_KMOD_GUI)
		{
			break;
		}
		if (ALLOW_QUIT && !event.key.repeat && event.key.keysym.sym == 'q' && (event.key.keysym.mod&SDL_KMOD_CTRL))
			engine.ConfirmExit();
		else
			engine.onKeyPress(event.key.keysym.sym, event.key.keysym.scancode, event.key.repeat, event.key.keysym.mod&SDL_KMOD_SHIFT, event.key.keysym.mod&SDL_KMOD_CTRL, event.key.keysym.mod&SDL_KMOD_ALT);
		break;
	case SDL_EVENT_KEY_UP:
		if (SDL_GetModState() & SDL_KMOD_GUI)
		{
			break;
		}
		engine.onKeyRelease(event.key.keysym.sym, event.key.keysym.scancode, event.key.repeat, event.key.keysym.mod&SDL_KMOD_SHIFT, event.key.keysym.mod&SDL_KMOD_CTRL, event.key.keysym.mod&SDL_KMOD_ALT);
		break;
	case SDL_EVENT_TEXT_INPUT:
		if (SDL_GetModState() & SDL_KMOD_GUI)
		{
			break;
		}
		engine.onTextInput(ByteString(event.text.text).FromUtf8());
		break;
	case SDL_EVENT_TEXT_EDITING:
		if (SDL_GetModState() & SDL_KMOD_GUI)
		{
			break;
		}
		engine.onTextEditing(ByteString(event.edit.text).FromUtf8(), event.edit.start);
		break;
	case SDL_EVENT_MOUSE_WHEEL:
	{
		// int x = event.wheel.x;
		int y = event.wheel.y;
		if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
		{
			// x *= -1;
			y *= -1;
		}

		engine.onMouseWheel(mousex, mousey, y); // TODO: pass x?
		break;
	}
	case SDL_EVENT_MOUSE_MOTION:
		mousex = event.motion.x;
		mousey = event.motion.y;
		engine.onMouseMove(mousex, mousey);

		hasMouseMoved = true;
		break;
	case SDL_EVENT_DROP_FILE:
		engine.onFileDrop(event.drop.data);
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		// if mouse hasn't moved yet, sdl will send 0,0. We don't want that
		if (hasMouseMoved)
		{
			mousex = event.button.x;
			mousey = event.button.y;
		}
		mouseButton = event.button.button;
		engine.onMouseDown(mousex, mousey, mouseButton);

		mouseDown = true;
		if constexpr (!DEBUG)
		{
			SDL_CaptureMouse(SDL_TRUE);
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		// if mouse hasn't moved yet, sdl will send 0,0. We don't want that
		if (hasMouseMoved)
		{
			mousex = event.button.x;
			mousey = event.button.y;
		}
		mouseButton = event.button.button;
		engine.onMouseUp(mousex, mousey, mouseButton);

		mouseDown = false;
		if constexpr (!DEBUG)
		{
			SDL_CaptureMouse(SDL_FALSE);
		}
		break;

	case SDL_EVENT_CLIPBOARD_UPDATE:
		Clipboard::FlushNative();
		break;
	}
}

void EngineProcess()
{
	auto &engine = ui::Engine::Ref();
	auto correctedFrameTime = frameStart - oldFrameStart;
	drawingTimer += correctedFrameTime;
	correctedFrameTimeAvg = correctedFrameTimeAvg + (correctedFrameTime - correctedFrameTimeAvg) * 0.05;
	if (correctedFrameTime && frameStart - lastFpsUpdate > UINT64_C(200'000'000))
	{
		engine.SetFps(1e9f / correctedFrameTimeAvg);
		lastFpsUpdate = frameStart;
	}
	if (frameStart - lastTick > UINT64_C(100'000'000))
	{
		lastTick = frameStart;
		TickClient();
	}
	if (showLargeScreenDialog)
	{
		showLargeScreenDialog = false;
		LargeScreenDialog();
	}

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		SDL_ConvertEventToRenderCoordinates(sdl_renderer, &event);
		EventProcess(event);
	}

	engine.Tick();

	auto fpsLimit = ui::Engine::Ref().GetFpsLimit();
	int drawcap = ui::Engine::Ref().GetDrawingFrequencyLimit();
	if (!drawcap || drawingTimer > 1e9f / drawcap)
	{
		engine.Draw();
		drawingTimer = 0;
		SDLSetScreen();
		blit(engine.g->Data());
	}
	auto now = uint64_t(SDL_GetTicks()) * UINT64_C(1'000'000);
	oldFrameStart = frameStart;
	frameStart = now;
	if (auto *fpsLimitExplicit = std::get_if<FpsLimitExplicit>(&fpsLimit))
	{
		auto timeBlockDuration = uint64_t(UINT64_C(1'000'000'000) / fpsLimitExplicit->value);
		auto oldFrameStartTimeBlock = oldFrameStart / timeBlockDuration;
		auto frameStartTimeBlock = oldFrameStartTimeBlock + 1U;
		frameStart = std::max(frameStart, frameStartTimeBlock * timeBlockDuration);
		SDL_Delay((frameStart - now) / UINT64_C(1'000'000));
	}
}
