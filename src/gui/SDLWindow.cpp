#include "SDLWindow.h"

#include "Config.h"
#include "Appearance.h"
#include "Event.h"
#include "ModalWindow.h"
#include "Texture.h"
#include "graphics/FontData.h"
#include "language/Language.h"
#include "windowicon.bmp.h"
#include "SDLAssert.h"

#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <vector>

#ifdef DEBUG
# include <iostream>
# include "Separator.h"
#endif

namespace gui
{
	constexpr auto backdropDarkenTicks = 300;
	constexpr auto backdropDarkenAlpha = 0.5f;
	constexpr auto scaleMargin         = 30;
	constexpr auto scaleMax            = 10;
	constexpr auto fontSurfaceSize     = 21;

	SDLWindow::SDLWindow(Configuration conf)
	{
		SDLASSERTZERO(SDL_InitSubSystem(SDL_INIT_VIDEO));
		SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
		SDL_StopTextInput();
		InitFontSurface();

		for (auto i = 0U; i < hoverCursors.size(); ++i)
		{
			hoverCursors[i] = SDLASSERTPTR(SDL_CreateSystemCursor(SDL_SystemCursor(i)));
		}

		Recreate(conf);
	}

	SDLWindow::~SDLWindow()
	{
		AssertModalWindowsEmpty();

		Close();
		SDL_FreeSurface(fontSurface);
		fontSurface = NULL;

		for (auto cursor : hoverCursors)
		{
			SDL_FreeCursor(cursor);
		}
		
		if (eglUnloadWorkaround)
		{
			// * nvidia-460 egl registers callbacks with x11 that end up being called
			//   after egl is unloaded unless we grab it here and release it after
			//   sdl closes the display. this is an nvidia driver weirdness but
			//   technically an sdl bug. glfw has this fixed:
			//   https://github.com/glfw/glfw/commit/9e6c0c747be838d1f3dc38c2924a47a42416c081
#ifdef DEBUG
			std::cerr << "applying EGL unload workaround" << std::endl;
#endif
			SDLASSERTZERO(SDL_GL_LoadLibrary(NULL));
		}
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		if (eglUnloadWorkaround)
		{
			SDL_GL_UnloadLibrary();
		}
		SDL_Quit();
	}

	void SDLWindow::AssertModalWindowsEmpty()
	{
		if (!modalWindows.empty())
		{
			throw std::runtime_error("modal window stack not empty");
		}
	}

	void SDLWindow::Close()
	{
		if (sdlWindow && (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_OPENGL))
		{
			eglUnloadWorkaround = true;
		}
		if (topLevelTexture)
		{
			SDL_DestroyTexture(topLevelTexture);
		}
		if (fontTexture)
		{
			SDL_DestroyTexture(fontTexture);
		}
		if (sdlRenderer)
		{
			SDL_DestroyRenderer(sdlRenderer);
		}
		if (sdlWindow)
		{
			SDL_DestroyWindow(sdlWindow);
		}
		topLevelTexture = NULL;
		fontTexture = NULL;
		sdlRenderer = NULL;
		sdlWindow = NULL;
	}

	void SDLWindow::Recreate(Configuration newConf)
	{
		AssertAllowed(forbidTreeOp | forbidGenerated);
		for (auto &mwe : modalWindows)
		{
			Event ev;
			ev.type = Event::RENDERERDOWN;
			ev.renderer.renderer = nullptr;
			mwe.window->HandleEvent(ev);
			if (mwe.backdrop)
			{
				SDL_DestroyTexture(mwe.backdrop);
			}
		}

		// * TODO-REDO_UI: If blurry upscaling is enabled, remember window size (and don't reset it
		//                 to baseSize * conf.scale) if the window already existed at this point.
		// * TODO-REDO_UI: If blurry upscaling is enabled, things that depend on conf.scale are going
		//                 to be incorrect; fix.
		Close();

		conf = newConf;
		if (conf.scale < 1)
		{
			conf.scale = 1;
		}
		if (conf.scale > scaleMax)
		{
			conf.scale = scaleMax;
		}

		unsigned int windowFlags = 0;
		if (conf.fullscreen)
		{
			windowFlags |= SDL_WINDOW_FULLSCREEN;
			if (conf.borderless)
			{
				windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
			}
		}
		else if (conf.resizable)
		{
			windowFlags |= SDL_WINDOW_RESIZABLE;
		}
		sdlWindow = SDLASSERTPTR(SDL_CreateWindow("DEFAULT_LS_GAME_INTRO_TITLE"_Ls().ToUtf8().c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, conf.size.x * conf.scale, conf.size.y * conf.scale, windowFlags));
		Uint32 rendererFlags = SDL_RENDERER_TARGETTEXTURE;
		switch (conf.rendererType)
		{
		case Configuration::rendererSoftware:
#ifdef DEBUG
			std::cerr << "forcing software renderer" << std::endl;
#endif
			rendererFlags |= SDL_RENDERER_SOFTWARE;
			break;

		case Configuration::rendererAccelerated:
#ifdef DEBUG
			std::cerr << "forcing accelerated renderer" << std::endl;
#endif
			rendererFlags |= SDL_RENDERER_ACCELERATED;
			break;

		default:
			break;
		}
		sdlRenderer = SDLASSERTPTR(SDL_CreateRenderer(sdlWindow, -1, rendererFlags));
		SDLASSERTZERO(SDL_RenderSetLogicalSize(sdlRenderer, conf.size.x, conf.size.y));
		ClipRect({ { 0, 0 }, { conf.size.x, conf.size.y } });
		fontTexture = SDLASSERTPTR(SDL_CreateTextureFromSurface(sdlRenderer, fontSurface));
		if (conf.integer && conf.fullscreen)
		{
			SDLASSERTZERO(SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE));
		}
		// * TODO-REDO_UI: Windows icon.
		SDL_Surface *iconSurface = SDLASSERTPTR(SDL_LoadBMP_RW(SDL_RWFromConstMem(windowIcon, windowIconSize), 1));
		SDL_SetWindowIcon(sdlWindow, iconSurface);
		SDL_FreeSurface(iconSurface);
		SDL_RaiseWindow(sdlWindow);
		SDLASSERTTRUE(SDL_SetHintWithPriority(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1", SDL_HINT_NORMAL));
		SDLASSERTZERO(SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND));
		SDLASSERTTRUE(SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, conf.blurry ? "1" : "0", SDL_HINT_NORMAL));
		topLevelTexture = SDLASSERTPTR(SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, conf.size.x, conf.size.y));
		SDLASSERTTRUE(SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "0", SDL_HINT_NORMAL));

		ModalWindowEntry *prevMwe = nullptr;
		for (auto &mwe : modalWindows)
		{
			Event ev;
			ev.type = Event::RENDERERUP;
			ev.renderer.renderer = sdlRenderer;
			mwe.window->HandleEvent(ev);
			if (mwe.backdrop)
			{
				mwe.backdrop = SDLASSERTPTR(SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, conf.size.x, conf.size.y));
				auto *prevRenderTarget = SDL_GetRenderTarget(sdlRenderer);
				SDLASSERTZERO(SDL_SetRenderTarget(sdlRenderer, mwe.backdrop));
				auto prevDrawToolTips = prevMwe->window->DrawToolTips();
				prevMwe->window->DrawToolTips(false);
				DrawWindowWithBackdrop(*prevMwe);
				prevMwe->window->DrawToolTips(prevDrawToolTips);
				SDLASSERTZERO(SDL_SetRenderTarget(sdlRenderer, prevRenderTarget));
			}
			prevMwe = &mwe;
		}
	}

	void SDLWindow::DrawTopWindow()
	{
		DrawWindowWithBackdrop(modalWindows.back());
	}

	void SDLWindow::PushBack(std::shared_ptr<ModalWindow> mw)
	{
		AssertAllowed(forbidTreeOp | forbidGenerated);
		if (mw->pushed)
		{
			throw std::runtime_error("attempt to push window already on the stack");
		}
		mw->pushed = true;
		SDL_Texture *backdrop = NULL;
		if (mw->WantBackdrop() && !modalWindows.empty())
		{
			backdrop = SDLASSERTPTR(SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, conf.size.x, conf.size.y));
			auto *prevRenderTarget = SDL_GetRenderTarget(sdlRenderer);
			SDLASSERTZERO(SDL_SetRenderTarget(sdlRenderer, backdrop));
			auto prevDrawToolTips = ModalWindowOnTop()->DrawToolTips();
			ModalWindowOnTop()->DrawToolTips(false);
			DrawTopWindow();
			ModalWindowOnTop()->DrawToolTips(prevDrawToolTips);
			SDLASSERTZERO(SDL_SetRenderTarget(sdlRenderer, prevRenderTarget));
		}
		if (!modalWindows.empty())
		{
			UpdateGainableStates(ModalWindowOnTop().get(), true);
		}
		modalWindows.push_back(ModalWindowEntry{ mw, backdrop, Ticks() });
		{
			Event ev;
			ev.type = Event::RENDERERUP;
			ev.renderer.renderer = sdlRenderer;
			mw->HandleEvent(ev);
		}
		UpdateGainableStates(mw.get(), false);
		discardTextInput = true;
	}

	void SDLWindow::PopBack(ModalWindow *mw)
	{
		AssertAllowed(forbidTreeOp | forbidGenerated);
		if (modalWindows.back().window.get() != mw)
		{
			throw std::runtime_error("attempt to pop window not on top of the stack");
		}
		UpdateGainableStates(mw, true);
		{
			Event ev;
			ev.type = Event::RENDERERDOWN;
			ev.renderer.renderer = nullptr;
			mw->HandleEvent(ev);
		}
		auto *backdrop = modalWindows.back().backdrop;
		if (backdrop)
		{
			SDL_DestroyTexture(backdrop);
		}
		mw->pushed = false;
		modalWindows.pop_back();
		if (!modalWindows.empty())
		{
			UpdateGainableStates(ModalWindowOnTop().get(), false);
		}
		discardTextInput = true;
	}

	void SDLWindow::UpdateGainableStates(ModalWindow *mw, bool spoofAllFalse)
	{
		bool withFocusReal = withFocus;
		bool underMouseReal = underMouse;
		if (spoofAllFalse)
		{
			withFocus = false;
			underMouse = false;
		}
		if (underMouse && !mw->UnderMouse())
		{
			Event ev;
			ev.type = Event::MOUSEENTER;
			ev.mouse.current = mousePosition;
			mw->HandleEvent(ev);
		}
		if (withFocus && !mw->WithFocus())
		{
			Event ev;
			ev.type = Event::FOCUSGAIN;
			mw->HandleEvent(ev);
		}
		if (underMouse)
		{
			Event ev;
			ev.type = Event::MOUSEMOVE;
			ev.mouse.current = mousePosition;
			mw->HandleEvent(ev);
		}
		if (!withFocus && mw->WithFocus())
		{
			Event ev;
			ev.type = Event::FOCUSLOSE;
			mw->HandleEvent(ev);
		}
		if (!underMouse && mw->UnderMouse())
		{
			Event ev;
			ev.type = Event::MOUSELEAVE;
			mw->HandleEvent(ev);
		}
		withFocus = withFocusReal;
		underMouse = underMouseReal;
	}

	void SDLWindow::FrameBegin()
	{
		frameStart = int32_t(SDL_GetTicks());
	}

	int32_t SDLWindow::Ticks() const
	{
		return int32_t(SDL_GetTicks());
	}

	void SDLWindow::FramePoll()
	{
		SDL_Event sdlEvent;
		while (SDL_PollEvent(&sdlEvent))
		{
			FrameEvent(sdlEvent);
			if (!Running())
			{
				return;
			}
		}
	}

	void SDLWindow::FrameEvent(const SDL_Event &sdlEvent)
	{
		auto mw = ModalWindowOnTop();
		Event ev;
		switch (sdlEvent.type)
		{
		case SDL_QUIT:
			if (mw->Quittable())
			{
				ev.type = Event::QUIT;
				mw->HandleEvent(ev);
				if (fastQuit)
				{
					while (true)
					{
						mw = ModalWindowOnTop();
						if (!mw)
						{
							break;
						}
						mw->HandleEvent(ev);
					}
				}
			}
			break;

		case SDL_KEYDOWN:
		case SDL_KEYUP:
			discardTextInput = false;
			ev.type = sdlEvent.type == SDL_KEYDOWN ? Event::KEYPRESS : Event::KEYRELEASE;
			ev.key.sym = sdlEvent.key.keysym.sym;
			ev.key.scan = sdlEvent.key.keysym.scancode;
			ev.key.repeat = sdlEvent.key.repeat;
			modShift = sdlEvent.key.keysym.mod & KMOD_SHIFT;
			modCtrl = sdlEvent.key.keysym.mod & KMOD_CTRL;
			modAlt = sdlEvent.key.keysym.mod & KMOD_ALT;
#ifdef DEBUG
			if (sdlEvent.type == SDL_KEYDOWN && !sdlEvent.key.repeat)
			{
				if (ev.key.sym == SDLK_SCROLLLOCK)
				{
					nudgingComponents = !nudgingComponents;
					drawComponentRects = nudgingComponents;
					drawContentRects = nudgingComponents;
					drawCharacterRects = nudgingComponents;
				}
				if (nudgingComponents)
				{
					auto posDelta = Point{ 0, 0 };
					auto sizeDelta = Point{ 0, 0 };
					auto *delta = modShift ? &sizeDelta : &posDelta;
					auto changed = false;
					auto insertChild = false;
					auto removeChild = false;
					if (ev.key.sym == SDLK_UP    ) { changed = true;      delta->y = -1; }
					if (ev.key.sym == SDLK_DOWN  ) { changed = true;      delta->y =  1; }
					if (ev.key.sym == SDLK_LEFT  ) { changed = true;      delta->x = -1; }
					if (ev.key.sym == SDLK_RIGHT ) { changed = true;      delta->x =  1; }
					if (ev.key.sym == SDLK_INSERT) { changed = true; insertChild = true; }
					if (ev.key.sym == SDLK_DELETE) { changed = true; removeChild = true; }
					if (modCtrl)
					{
						*delta *= 10;
					}
					if (changed)
					{
						auto mw = ModalWindowOnTop();
						if (mw)
						{
							auto *child = mw->ChildUnderMouseDeep();
							if (!child)
							{
								child = mw.get();
							}
							child->Position(child->Position() + posDelta);
							child->Size(child->Size() + sizeDelta);
							auto pos = child->Position();
							auto size = child->Size();
							std::cerr << "{ " << pos.x << ", " << pos.y << " } { " << size.x << ", " << size.y << " }" << std::endl;
							if (removeChild)
							{
								child->Parent()->RemoveChild(child);
							}
							if (insertChild)
							{
								auto *sep = child->EmplaceChild<Separator>().get();
								sep->Position(MousePosition() - child->AbsolutePosition());
								sep->Size({ 5, 5 });
							}
						}
						break;
					}
				}
			}
#endif
			UpdateModState();
			ev.key.shift = modShift;
			ev.key.ctrl = modCtrl;
			ev.key.alt = modAlt;
			mw->HandleEvent(ev);
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			mouseButtonsDown += sdlEvent.type == SDL_MOUSEBUTTONDOWN ? 1 : -1;
#if !defined(AND) && !defined(DEBUG)
			SDLASSERTZERO(SDL_CaptureMouse(mouseButtonsDown > 0 ? SDL_TRUE : SDL_FALSE));
#endif
			ev.type = sdlEvent.type == SDL_MOUSEBUTTONDOWN ? Event::MOUSEDOWN : Event::MOUSEUP;
			ev.mouse.button = sdlEvent.button.button;
			ev.mouse.clicks = sdlEvent.button.clicks;
			mw->HandleEvent(ev);
			break;

		case SDL_MOUSEMOTION:
			mousePosition = { sdlEvent.motion.x, sdlEvent.motion.y };
			ev.type = Event::MOUSEMOVE;
			ev.mouse.current = mousePosition;
			mw->HandleEvent(ev);
			break;

		case SDL_MOUSEWHEEL:
			// * Forward these as two separate events instead of one. The reason is that the
			//   most important consumer of these events is ScrollPanels, which need to be
			//   able to let the event bubble up in case they can't scroll any further.
			// * Forwarding both directions as one event would mean that a ScrollPanel
			//   would have to either consume or ignore scrolling in both directions, which
			//   would be bad if, for example, the scroll panel could scroll further
			//   horizontally but not vertically.
			// * It's not too intuitive, but the direction of scrolling is stored
			//   in the .button member; 0 for horizontal, 1 for vertical.
			if (sdlEvent.wheel.x)
			{
				ev.type = Event::MOUSEWHEEL;
				ev.mouse.wheel = sdlEvent.wheel.x;
				ev.mouse.button = 0;
				if (sdlEvent.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
				{
					ev.mouse.wheel *= -1;
				}
				mw->HandleEvent(ev);
			}
			if (sdlEvent.wheel.y)
			{
				ev.type = Event::MOUSEWHEEL;
				ev.mouse.wheel = sdlEvent.wheel.y;
				ev.mouse.button = 1;
				if (sdlEvent.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
				{
					ev.mouse.wheel *= -1;
				}
				mw->HandleEvent(ev);
			}
			break;

		case SDL_WINDOWEVENT:
			switch (sdlEvent.window.event)
			{
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				{
					// TODO-REDO_UI: Recalculate candidate window position based on a real rendered-to-window transform, see RendererRect().
				}
				break;

			case SDL_WINDOWEVENT_ENTER:
				underMouse = true;
				{
					Point global, window;
					SDL_GetGlobalMouseState(&global.x, &global.y);
					SDL_GetWindowPosition(sdlWindow, &window.x, &window.y);
					mousePosition = (global - window) / conf.scale; // * TODO-REDO_UI: Use real rendered-to-window transform, see RendererRect().
					ev.type = Event::MOUSEENTER;
					ev.mouse.current = mousePosition;
				}
				mw->HandleEvent(ev);
				break;
			
			case SDL_WINDOWEVENT_LEAVE:
				ev.type = Event::MOUSELEAVE;
				mw->HandleEvent(ev);
				underMouse = false;
				break;
			
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				withFocus = true;
				ev.type = Event::FOCUSGAIN;
				mw->HandleEvent(ev);
				break;
			
			case SDL_WINDOWEVENT_FOCUS_LOST:
				modShift = false;
				modCtrl = false;
				modAlt = false;
				UpdateModState();
				ev.type = Event::FOCUSLOSE;
				mw->HandleEvent(ev);
				withFocus = false;
				break;
			}
			break;

		case SDL_TEXTINPUT:
			if (discardTextInput)
			{
				break;
			}
			{
				String text = ByteString(sdlEvent.text.text).FromUtf8();
				ev.type = Event::TEXTINPUT;
				ev.text.data = &text;
				mw->HandleEvent(ev);
			}
			break;

		case SDL_TEXTEDITING:
			if (discardTextInput)
			{
				break;
			}
			{
				String text = ByteString(sdlEvent.text.text).FromUtf8();
				if (!sdlEvent.edit.start)
				{
					textEditingBuffer.clear();
				}
				// * sdlEvent.edit.length is a joke and shouldn't be taken seriously.
				if (int(textEditingBuffer.size()) != sdlEvent.edit.start)
				{
					throw std::runtime_error(ByteString::Build(
						"SDL_TEXTEDITING is being weird\n",
						"sdlEvent.edit.start: ", sdlEvent.edit.start, "\n",
						"sdlEvent.edit.length: ", sdlEvent.edit.length, "\n",
						"sdlEvent.text.text: '", sdlEvent.text.text, "'\n",
						"textEditingBuffer.size(): ", textEditingBuffer.size(), "\n",
						"textEditingBuffer: '", textEditingBuffer.ToUtf8(), "'"
					));
				}
				textEditingBuffer.append(text);
				ev.type = Event::TEXTEDITING;
				ev.text.data = &textEditingBuffer;
				mw->HandleEvent(ev);
			}
			break;

		case SDL_DROPFILE:
			{
				String path = ByteString(sdlEvent.drop.file).FromUtf8();
				ev.type = Event::FILEDROP;
				ev.filedrop.path = &path;
				mw->HandleEvent(ev);
			}
			SDL_free(sdlEvent.drop.file);
			break;
		}
	}

	void SDLWindow::TickTopWindow()
	{
		auto mw = ModalWindowOnTop();
		Event ev;
		timeSinceRender += (frameStart - prevFrameStart) / 1000.f;
		prevFrameStart = frameStart;
		ev.type = Event::TICK;
		mw->HandleEvent(ev);
	}

	void SDLWindow::DrawWindowWithBackdrop(const ModalWindowEntry &mwe)
	{
		SDLASSERTZERO(SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255));
		SDLASSERTZERO(SDL_RenderClear(sdlRenderer));
		if (mwe.backdrop)
		{
			int32_t pushedFor = Ticks() - mwe.pushedAt;
			auto alpha = int(255.f * (1.f - backdropDarkenAlpha));
			if (pushedFor < backdropDarkenTicks)
			{
				alpha = int(255.f * (1.f - powf(backdropDarkenAlpha, pushedFor / float(backdropDarkenTicks))));
			}
			SDLASSERTZERO(SDL_RenderCopy(sdlRenderer, mwe.backdrop, NULL, NULL));
			SDLASSERTZERO(SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, alpha));
			SDLASSERTZERO(SDL_RenderFillRect(sdlRenderer, NULL));
		}
		Event ev;
		ev.type = Event::DRAW;
		static_cast<const ModalWindow *>(mwe.window.get())->HandleEvent(ev);
	}

	void SDLWindow::UpdateHoverCursorIfNeeded()
	{
		if (!shouldUpdateHoverCursor)
		{
			return;
		}
		shouldUpdateHoverCursor = false;
		auto hoverCursor = Cursor::cursorArrow;
		auto *child = ModalWindowOnTop()->ChildUnderMouseDeep();
		if (child)
		{
			hoverCursor = child->HoverCursor();
			if (int(hoverCursor) < 0 || int(hoverCursor) >= int(Cursor::cursorMax))
			{
				hoverCursor = Cursor::cursorArrow;
			}
		}
		if (lastHoverCursor != hoverCursor)
		{
			auto *cursor = hoverCursors[hoverCursor];
			if (!cursor)
			{
				cursor = hoverCursors[Cursor::cursorArrow];
			}
			SDL_SetCursor(cursor);
			lastHoverCursor = hoverCursor;
		}
	}

	void SDLWindow::FrameTick()
	{
		TickTopWindow();
		UpdateHoverCursorIfNeeded();
	}

	void SDLWindow::FrameDraw()
	{
		int rpsLimit = ModalWindowOnTop()->RPSLimit();
		if (!rpsLimit || timeSinceRender > 1.f / rpsLimit)
		{
			timeSinceRender = 0;
			auto *prevRenderTarget = SDL_GetRenderTarget(sdlRenderer);
			SDLASSERTZERO(SDL_SetRenderTarget(sdlRenderer, topLevelTexture));
			DrawTopWindow();
			SDLASSERTZERO(SDL_SetRenderTarget(sdlRenderer, prevRenderTarget));
			SDLASSERTZERO(SDL_RenderCopy(sdlRenderer, topLevelTexture, NULL, NULL));
		}
	}

	void SDLWindow::FrameEnd()
	{
		SDL_RenderPresent(sdlRenderer);
		int32_t frameTime = Ticks() - frameStart;
		frameTimeAvg = frameTimeAvg * 0.8 + frameTime * 0.2;
	}

	void SDLWindow::FrameDelay()
	{
		auto mw = ModalWindowOnTop();
		if (mw->FPSLimit() > 2)
		{
			double offset = 1000.0 / mw->FPSLimit() - frameTimeAvg;
			if (offset > 0)
			{
				SDL_Delay(Uint32(offset + 0.5));
			}
		}
	}

	void SDLWindow::FrameTime()
	{
		int32_t correctedFrameTime = SDL_GetTicks() - frameStart;
		correctedFrameTimeAvg = correctedFrameTimeAvg * 0.95 + correctedFrameTime * 0.05;
		if (frameStart - lastFpsUpdate > 200)
		{
			fpsMeasured = 1000.0 / correctedFrameTimeAvg;
			lastFpsUpdate = frameStart;
		}
	}

	void SDLWindow::ClipRect(Rect newClipRect)
	{
		clipRect = newClipRect;
		SDL_Rect sdlRect;
		SDL_Rect *effective = NULL;
		if (!(clipRect.pos == Point{ 0, 0 } && clipRect.size == conf.size))
		{
			sdlRect.x = clipRect.pos.x;
			sdlRect.y = clipRect.pos.y;
			sdlRect.w = clipRect.size.x;
			sdlRect.h = clipRect.size.y;
			effective = &sdlRect;
		}
		SDLASSERTZERO(SDL_RenderSetClipRect(sdlRenderer, effective));
	}

	void SDLWindow::DrawLine(Point from, Point to, Color color)
	{
		SDLASSERTZERO(SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a));
		SDLASSERTZERO(SDL_RenderDrawLine(sdlRenderer, from.x, from.y, to.x, to.y));
	}
	
	void SDLWindow::DrawRect(Rect rect, Color color)
	{
		SDLASSERTZERO(SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a));
		SDL_Rect sdlRect;
		sdlRect.x = rect.pos.x;
		sdlRect.y = rect.pos.y;
		sdlRect.w = rect.size.x;
		sdlRect.h = rect.size.y;
		SDLASSERTZERO(SDL_RenderFillRect(sdlRenderer, &sdlRect));
	}

	void SDLWindow::DrawRectOutline(Rect rect, Color color)
	{
		SDLASSERTZERO(SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a));
		SDL_Rect sdlRect;
		sdlRect.x = rect.pos.x;
		sdlRect.y = rect.pos.y;
		sdlRect.w = rect.size.x;
		sdlRect.h = rect.size.y;
		SDLASSERTZERO(SDL_RenderDrawRect(sdlRenderer, &sdlRect));
	}
	
	void SDLWindow::DrawTexture(const Texture &texture, Rect rect)
	{
		DrawTexture(texture, rect, { { 0, 0 }, texture.ImageData().Size() });
	}
	
	void SDLWindow::DrawTexture(const Texture &texture, Rect rect, Rect source)
	{
		if (texture.Handle())
		{
			SDL_Rect sdlSrc;
			sdlSrc.x = source.pos.x;
			sdlSrc.y = source.pos.y;
			sdlSrc.w = source.size.x;
			sdlSrc.h = source.size.y;
			SDL_Rect sdlDest;
			sdlDest.x = rect.pos.x;
			sdlDest.y = rect.pos.y;
			sdlDest.w = rect.size.x;
			sdlDest.h = rect.size.y;
			SDLASSERTZERO(SDL_RenderCopy(sdlRenderer, texture.Handle(), &sdlSrc, &sdlDest));
		}
	}
	
	uint32_t SDLWindow::GlyphData(int ch) const
	{
		uint32_t pos = 0;
		for (auto &range : fontGlyphRanges)
		{
			auto begin = std::get<0>(range);
			auto end = std::get<1>(range);
			if (ch >= begin && ch < end)
			{
				return fontGlyphData[pos + ch - begin];
			}
			pos += end - begin;
		}
		if (ch == 0xFFFD)
		{
			return 0;
		}
		else
		{
			return GlyphData(0xFFFD);
		}
	}

	void SDLWindow::InitFontSurface()
	{
		auto &fd = GetFontData();
		int chars = 0;
		for (int r = 0; !(!fd.ranges[r][0] && !fd.ranges[r][1]); ++r)
		{
			auto begin = fd.ranges[r][0];
			auto end = fd.ranges[r][1] + 1;
			fontGlyphRanges.push_back(std::make_tuple(begin, end));
			chars += end - begin;
		}
		fontGlyphData.resize(chars);
		static_assert(fontSurfaceSize <= 28, "fontSurfaceSize too big");
		uint32_t textureWidth = 1 << (fontSurfaceSize - fontSurfaceSize / 2);
		uint32_t textureHeight = 1 << (fontSurfaceSize / 2);
		fontSurface = SDLASSERTPTR(SDL_CreateRGBSurfaceWithFormat(0, textureWidth, textureHeight, 32, SDL_PIXELFORMAT_ARGB8888));
		auto dataIt = fontGlyphData.begin();
		auto *ptrs = fd.ptrs;
		uint32_t atX = 0;
		uint32_t atY = 0;
		SDLASSERTZERO(SDL_LockSurface(fontSurface));
		for (auto &range : fontGlyphRanges)
		{
			for (auto i = std::get<0>(range); i < std::get<1>(range); ++i)
			{
				int ptr = *(ptrs++);
				uint32_t &gd = *(dataIt++);
				uint32_t charWidth = fd.bits[ptr++];
				if (atX + charWidth > textureWidth)
				{
					atX = 0;
					atY += FONT_H;
				}
				if (atY + FONT_H > textureHeight)
				{
					// * If you get this, increase fontSurfaceSize above.
					throw std::runtime_error("fontSurfaceSize too small");
				}
				gd = charWidth | (atX << 4) | (atY << 18);
				int bits = 0;
				int buffer;
				char *pixels = reinterpret_cast<char *>(fontSurface->pixels);
				for (uint32_t y = 0; y < FONT_H; ++y)
				{
					for (uint32_t x = 0; x < charWidth; ++x)
					{
						if (!bits)
						{
							buffer = fd.bits[ptr++];
							bits = 8;
						}
						reinterpret_cast<uint32_t *>(&pixels[(atY + y) * fontSurface->pitch])[atX + x] = ((buffer & 3) * UINT32_C(0x55000000)) | UINT32_C(0xFFFFFF);
						bits -= 2;
						buffer >>= 2;
					}
				}
				atX += charWidth;
			}
		}
		SDL_UnlockSurface(fontSurface);
	}

	void SDLWindow::DrawText(Point position, const String &text, Color initial, uint32_t flags)
	{
		if (flags & drawTextShaded)
		{
			auto size = TextSize(text);
			DrawRect({ position - gui::Point{ 4, 2 }, size + gui::Point{ 7, 3 } }, { 0, 0, 0, initial.a * 160 / 255 });
		}
		SDLASSERTZERO(SDL_SetTextureBlendMode(fontTexture, SDL_BLENDMODE_BLEND));
		SDLASSERTZERO(SDL_SetTextureAlphaMod(fontTexture, initial.a));
		Point drawAt = position;
		auto nextOffset = Point{ 0, 0 };
		bool canBacktrack = false;
		bool bold      = flags & drawTextBold;
		bool invert    = flags & drawTextInvert;
		bool darken    = flags & drawTextDarken;
		bool underline = flags & drawTextUnderline;
		Color color;
		auto setColor = [this, &invert, &darken, &color](Color newColor) {
			color = newColor;
			if (invert)
			{
				newColor.r = 255 - newColor.r;
				newColor.g = 255 - newColor.g;
				newColor.b = 255 - newColor.b;
			}
			if (darken)
			{
				newColor.r /= 2;
				newColor.g /= 2;
				newColor.b /= 2;
			}
			SDLASSERTZERO(SDL_SetTextureColorMod(fontTexture, newColor.r, newColor.g, newColor.b));
		};
		setColor(initial);
		int backtrackBy = 0;
		int monospace = (flags & drawTextMonospaceMask) >> drawTextMonospaceShift;
		auto it = text.begin();
		while (it != text.end())
		{
			auto ch = *it;
			if (ch == '\n')
			{
				drawAt.x = position.x;
				drawAt.y += FONT_H;
				canBacktrack = false;
			}
			else if (ch == '\b' && it + 1 < text.end())
			{
				auto escape = *(++it);
				switch (escape)
				{
				case 'w': setColor(Appearance::commonWhite    ); break;
				case 'g': setColor(Appearance::commonLightGray); break;
				case 'o': setColor(Appearance::commonYellow   ); break;
				case 'r': setColor(Appearance::commonRed      ); break;
				case 'l': setColor(Appearance::commonLightRed ); break;
				case 'b': setColor(Appearance::commonBlue     ); break;
				case 't': setColor(Appearance::commonLightBlue); break;
				case 'u': setColor(Appearance::commonPurple   ); break;

				case 'd':
					bold = true;
					break;

				case 'n':
					underline = true;
					break;

				case 'a':
					if (!invert)
					{
						SDLASSERTZERO(SDL_SetTextureBlendMode(fontTexture, SDL_BLENDMODE_ADD));
					}
					break;

				case 'z':
					bold      = flags & drawTextBold;
					invert    = flags & drawTextInvert;
					darken    = flags & drawTextDarken;
					underline = flags & drawTextUnderline;
					setColor(initial);
					break;

				case 'i':
					invert = !invert;
					setColor(color);
					break;

				case 'k':
					darken = !darken;
					setColor(color);
					break;

				case 'x':
					if (canBacktrack)
					{
						drawAt.x -= backtrackBy;
					}
					break;

				default:
					if (escape >= 0xC0 && escape < 0x100)
					{
						nextOffset.x = escape & 7;
						nextOffset.y = (escape & 56) >> 3;
						if (nextOffset.x & 4) nextOffset.x |= ~7;
						if (nextOffset.y & 4) nextOffset.y |= ~7;
					}
					if (escape >= 0x100 && escape < 0x200)
					{
						int offset = escape & 0xFF;
						if (offset & 0x80)
						{
							offset -= 0x100;
						}
						if (underline)
						{
							DrawLine(drawAt + Point{ 0, FONT_H + 1 }, drawAt + Point{ offset - 1, FONT_H + 1 }, color);
						}
						drawAt.x += offset;
					}
					if (escape >= 0xA0 && escape < 0xB0)
					{
						monospace = escape & 15;
					}
					break;
				}
				canBacktrack = false;
			}
			else if (ch == '\x0F' && it + 3 < text.end())
			{
				auto r = *(++it);
				auto g = *(++it);
				auto b = *(++it);
				setColor({ int(r), int(g), int(b) });
				canBacktrack = false;
			}
			else
			{
				uint32_t gd = GlyphData(ch);
				SDL_Rect rectFrom, rectTo;
				rectFrom.x = (gd & UINT32_C(0x0003FFF0)) >> 4;
				rectFrom.y = (gd & UINT32_C(0xFFFC0000)) >> 18;
				rectFrom.w = gd & UINT32_C(0x0000000F);
				rectFrom.h = FONT_H;
				rectTo.x = drawAt.x + nextOffset.x;
				rectTo.y = drawAt.y + nextOffset.y;
				if (underline)
				{
					DrawLine(drawAt + Point{ 0, FONT_H + 1 }, drawAt + Point{ rectFrom.w - 1, FONT_H + 1 }, color);
				}
				rectTo.w = rectFrom.w;
				rectTo.h = rectFrom.h;
				if (monospace)
				{
					int diff = monospace - rectFrom.w;
					rectTo.x += diff - diff / 2;
					drawAt.x += monospace;
				}
				else
				{
					drawAt.x += rectFrom.w;
				}
				SDLASSERTZERO(SDL_RenderCopy(sdlRenderer, fontTexture, &rectFrom, &rectTo));
#ifdef DEBUG
				if (drawCharacterRects)
				{
					DrawRectOutline({ { rectTo.x, rectTo.y }, { rectTo.w, rectTo.h } }, { 0, 255, 0, 128 });
				}
#endif
				if (bold)
				{
					rectTo.x += 1;
					SDLASSERTZERO(SDL_RenderCopy(sdlRenderer, fontTexture, &rectFrom, &rectTo));
				}
				canBacktrack = true;
				backtrackBy = monospace ? monospace : rectFrom.w;
				nextOffset = { 0, 0 };
			}
			++it;
		}
	}

	Point SDLWindow::TextSize(const String &text, uint32_t flags) const
	{
		Point result;
		FitText(result, text, -1, flags);
		return result;
	}

	int SDLWindow::FitText(Point &size, const String &text, int limit, uint32_t flags) const
	{
		int prefix = 0;
		int boxWidth = 0;
		int boxHeight = FONT_H;
		int lineWidth = 0;
		bool canBacktrack = false;
		int backtrackBy = 0;
		int monospace = (flags & drawTextMonospaceMask) >> drawTextMonospaceShift;
		auto it = text.begin();
		while (it != text.end())
		{
			auto ch = *it;
			if (ch == '\n')
			{
				if (limit >= 0)
				{
					break;
				}
				lineWidth = 0;
				boxHeight += FONT_H;
				canBacktrack = false;
			}
			else if (ch == '\b' && it + 1 < text.end())
			{
				auto escape = *(++it);
				auto breakLoop = false;
				switch (escape)
				{
				case 'x':
					if (canBacktrack)
					{
						lineWidth -= backtrackBy;
					}
					break;

				default:
					if (escape >= 0x100 && escape < 0x200)
					{
						int offset = escape & 0xFF;
						if (offset & 0x80)
						{
							offset -= 0x100;
						}
						lineWidth += offset;
						if (limit >= 0 && limit < lineWidth)
						{
							breakLoop = true;
							break;
						}
						if (boxWidth < lineWidth)
						{
							boxWidth = lineWidth;
						}
					}
					if (escape >= 0xA0 && escape < 0xB0)
					{
						monospace = escape & 15;
					}
					break;
				}
				if (breakLoop)
				{
					break;
				}
				canBacktrack = false;
			}
			else if (ch == '\x0F' && it + 3 < text.end())
			{
				it += 3;
				canBacktrack = false;
			}
			else
			{
				auto charWidth = GlyphData(ch) & UINT32_C(0x0000000F);
				lineWidth += monospace ? monospace : charWidth;
				if (limit >= 0 && limit < lineWidth)
				{
					break;
				}
				if (boxWidth < lineWidth)
				{
					boxWidth = lineWidth;
				}
				canBacktrack = true;
				backtrackBy = monospace ? monospace : charWidth;
			}
			++it;
			prefix = it - text.begin();
		}
		size = { boxWidth, boxHeight };
		return prefix;
	}

	int SDLWindow::TruncateText(String &truncated, Point &size, const String &text, int limit, const String &overflowText, uint32_t flags) const
	{
		size = TextSize(text, flags);
		if (size.x <= limit)
		{
			truncated = text;
			return text.size();
		}
		// * Assumption: No pitch-changing nonsense goes on in the original text, and it's
		//               not drawn with pitch-changing nonsense flags. The idea is that
		//               none of those features should be important if you're trying to
		//               truncate overflowing text.
		auto ots = TextSize(overflowText);
		if (ots.x <= limit)
		{
			auto used = FitText(size, text, limit - ots.x, flags);
			truncated = text.Substr(0, used) + overflowText;
			size.x += ots.x;
			return used;
		}
		size = { 0, 0 };
		truncated.clear();
		return 0;
	}

	void SDLWindow::ClipboardText(const String &text)
	{
		SDLASSERTZERO(SDL_SetClipboardText(text.ToUtf8().c_str()));
	}

	String SDLWindow::ClipboardText()
	{
		return ByteString(SDLASSERTPTR(SDL_GetClipboardText())).FromUtf8();
	}

	bool SDLWindow::ClipboardEmpty()
	{
		return !SDL_HasClipboardText();
	}

	int SDLWindow::CharWidth(int ch) const
	{
		return GlyphData(ch) & 0x0000000FU;
	}

	int SDLWindow::MaxScale() const
	{
		int index = SDL_GetWindowDisplayIndex(sdlWindow);
		if (index < 0)
		{
			return 1;
		}
		SDL_Rect rect;
		if (SDL_GetDisplayUsableBounds(index, &rect) < 0)
		{
			return 1;
		}
		auto maxW = (rect.w - scaleMargin) / WINDOWW;
		auto maxH = (rect.h - scaleMargin) / WINDOWH;
		auto max = scaleMax;
		if (max > maxW)
		{
			max = maxW;
		}
		if (max > maxH)
		{
			max = maxH;
		}
		if (max < 1)
		{
			max = 1;
		}
		return max;
	}

	void SDLWindow::StartTextInput()
	{
		if (textInput)
		{
			return;
		}
		textInput = true;
		SDL_StartTextInput();
	}

	void SDLWindow::StopTextInput()
	{
		if (!textInput)
		{
			return;
		}
		textInput = false;
		SDL_StopTextInput();
	}

	void SDLWindow::TextInputRect(Rect rect)
	{
		SDL_Rect sdlRect;
		// * TODO-REDO_UI: Make sure this transformation always works, even in e.g. fullscreen.
		//                 SDL really should do this for us, because it has all the info, but we don't.
		// * TODO-REDO_UI: Use real rendered-to-window transform, see RendererRect().
		sdlRect.x = rect.pos.x * conf.scale;
		sdlRect.y = rect.pos.y * conf.scale;
		sdlRect.w = rect.size.x * conf.scale;
		sdlRect.h = rect.size.y * conf.scale;
		SDL_SetTextInputRect(&sdlRect);
	}

	gui::Rect SDLWindow::RendererRect() const
	{
		gui::Rect rect;
		SDLASSERTZERO(SDL_GetRendererOutputSize(sdlRenderer, &rect.size.x, &rect.size.y));
		SDL_Rect sdlRect;
		SDL_RenderGetViewport(sdlRenderer, &sdlRect);
		rect.pos = { sdlRect.x, sdlRect.y };
		return rect;
	}

	ByteString SDLWindow::GetPrefPath()
	{
		char *ddir = SDL_GetPrefPath(NULL, "The Powder Toy");
		if (ddir)
		{
			auto ret = ByteString(ddir);
			SDL_free(ddir);
			return ret;
		}
		return ByteString("");
	}

	void SDLWindow::UpdateModState()
	{
		modState = 0;
		// * TODO-REDO_UI: Make it possible to remap these.
		if (modShift) modState |= mod0;
		if (modCtrl ) modState |= mod1;
		if (modAlt  ) modState |= mod2;
	}

	SDLWindow::ForbidMethods::ForbidMethods(uint32_t newForbiddenMethods)
	{
		prevForbiddenMethods = SDLWindow::Ref().forbiddenMethods;
		SDLWindow::Ref().forbiddenMethods |= newForbiddenMethods;
	}

	SDLWindow::ForbidMethods::~ForbidMethods()
	{
		SDLWindow::Ref().forbiddenMethods = prevForbiddenMethods;
	}

	void SDLWindow::BlueScreen(String details)
	{
		StringBuilder sb;
		sb << "ERROR\n\n";
		sb << "Details: " << details << "\n\n";
		sb << "An unrecoverable fault has occurred, please report the error by visiting the website below\n";
		sb << SCHEME SERVER;
		String message = sb.Build();
#if defined(AND)
		SDL_Log("%s\n", message.ToUtf8().c_str());
		while (true)
		{
			SDL_Delay(20);
		}
#else
		DrawRect({ { 0, 0 }, Configuration().size }, { 17, 114, 169, 210 });
		DrawText((Configuration().size - TextSize(message)) / 2, message, { 255, 255, 255, 255 });
		FrameEnd();
		while (true)
		{
			SDL_Event sdlEvent;
			while (SDL_PollEvent(&sdlEvent))
			{
				if (sdlEvent.type == SDL_QUIT)
				{
					exit(-1);
				}
			}
			SDL_Delay(20);
		}
#endif
	}

	void SDLWindow::AssertAllowed(uint32_t methodsToCheck)
	{
		if (forbiddenMethods & methodsToCheck)
		{
			throw std::runtime_error("this function cannot be called now");
		}
	}
}
