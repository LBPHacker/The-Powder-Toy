#include "SDLWindow.h"

#include "Config.h"
#include "Appearance.h"
#include "Event.h"
#include "ModalWindow.h"
#include "icon.h"
#include "graphics/FontData.h"

#include <SDL2/SDL.h>
#include <stdexcept>
#include <vector>
#include <algorithm>

namespace gui
{
	constexpr auto backdropDarkenTicks = 300;
	constexpr auto backdropDarkenAlpha = 85;
	constexpr auto scaleMargin = 30;
	constexpr auto scaleMax = 10;
	constexpr char windowTitle[] = "The Powder Toy";
	constexpr auto textureSize = 21;

	SDLWindow::SDLWindow(Configuration conf)
	{
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		{
			throw std::runtime_error(ByteString("SDL_InitSubSystem(SDL_INIT_VIDEO) failed\n") + SDL_GetError());
		}
		SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
		InitFontSurface();

		Recreate(conf);
	}

	SDLWindow::~SDLWindow()
	{
		AssertModalWindowsEmpty();

		Close();
		SDL_FreeSurface(fontSurface);
		fontSurface = NULL;
		
		if (eglUnloadWorkaround)
		{
			// * nvidia-460 egl registers callbacks with x11 that end up being called
			//   after egl is unloaded unless we grab it here and release it after
			//   sdl closes the display. this is an nvidia driver weirdness but
			//   technically an sdl bug. glfw has this fixed:
			//   https://github.com/glfw/glfw/commit/9e6c0c747be838d1f3dc38c2924a47a42416c081
			SDL_GL_LoadLibrary(NULL);
		}
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		if (eglUnloadWorkaround)
		{
			SDL_GL_UnloadLibrary();
		}
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
		if (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_OPENGL)
		{
			eglUnloadWorkaround = true;
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
		fontTexture = NULL;
		sdlRenderer = NULL;
		sdlWindow = NULL;
	}

	void SDLWindow::Recreate(Configuration newConf)
	{
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

		unsigned int flags = 0;
		if (conf.fullscreen)
		{
			flags |= SDL_WINDOW_FULLSCREEN;
		}
		else if (conf.borderless)
		{
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
		else if (conf.resizable)
		{
			flags |= SDL_WINDOW_RESIZABLE;
		}
		sdlWindow = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, conf.size.x * conf.scale, conf.size.y * conf.scale, flags);
		if (!sdlWindow)
		{
			throw std::runtime_error(ByteString("SDL_CreateWindow failed\n") + SDL_GetError());
		}
		sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_TARGETTEXTURE);
		if (!sdlRenderer)
		{
			throw std::runtime_error(ByteString("SDL_CreateRenderer failed\n") + SDL_GetError());
		}
		SDL_RenderSetLogicalSize(sdlRenderer, conf.size.x, conf.size.y);
		ClipRect(Rect{ Point{ 0, 0 }, Point{ conf.size.x, conf.size.y } });
		fontTexture = SDL_CreateTextureFromSurface(sdlRenderer, fontSurface);
		if (!fontTexture)
		{
			throw std::runtime_error(ByteString("SDL_CreateTextureFromSurface failed\n") + SDL_GetError());
		}
		if (conf.integer && conf.fullscreen)
		{
			SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE);
		}
		// * Let's just hope that SDL won't try to write the surface data.
		SDL_Surface *iconSurface = SDL_CreateRGBSurfaceFrom((void *)app_icon, 128, 128, 32, 512, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
		SDL_SetWindowIcon(sdlWindow, iconSurface);
		SDL_FreeSurface(iconSurface);
		SDL_RaiseWindow(sdlWindow);
		if (SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND))
		{
			throw std::runtime_error(ByteString("SDL_SetRenderDrawBlendMode failed\n") + SDL_GetError());
		}

		ModalWindow *prevWindow = nullptr;
		SDL_Texture *prevBackdrop = NULL;
		for (auto &mwe : modalWindows)
		{
			Event ev;
			ev.type = Event::RENDERERUP;
			ev.renderer.renderer = sdlRenderer;
			mwe.window->HandleEvent(ev);
			if (mwe.backdrop)
			{
				mwe.backdrop = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, conf.size.x, conf.size.y);
				if (!mwe.backdrop)
				{
					throw std::runtime_error(ByteString("SDL_CreateTexture failed\n") + SDL_GetError());
				}
				auto *prevRenderTarget = SDL_GetRenderTarget(sdlRenderer);
				if (SDL_SetRenderTarget(sdlRenderer, mwe.backdrop))
				{
					throw std::runtime_error(ByteString("SDL_SetRenderTarget failed\n") + SDL_GetError());
				}
				DrawWindowWithBackdrop(prevWindow, prevBackdrop, backdropDarkenAlpha);
				SDL_SetRenderTarget(sdlRenderer, prevRenderTarget);
			}
			prevWindow = mwe.window.get();
			prevBackdrop = mwe.backdrop;
		}
	}

	void SDLWindow::PushBack(std::shared_ptr<ModalWindow> mw)
	{
		if (mw->pushed)
		{
			throw std::runtime_error("attempt to push window already on the stack");
		}
		mw->pushed = true;
		SDL_Texture *backdrop = NULL;
		if (mw->WantsBackdrop() && !modalWindows.empty())
		{
			backdrop = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, conf.size.x, conf.size.y);
			if (!backdrop)
			{
				throw std::runtime_error(ByteString("SDL_CreateTexture failed\n") + SDL_GetError());
			}
			auto *prevRenderTarget = SDL_GetRenderTarget(sdlRenderer);
			if (SDL_SetRenderTarget(sdlRenderer, backdrop))
			{
				throw std::runtime_error(ByteString("SDL_SetRenderTarget failed\n") + SDL_GetError());
			}
			auto prevDrawToolTips = ModalWindowOnTop()->DrawToolTips();
			ModalWindowOnTop()->DrawToolTips(false);
			DrawTopWindow();
			ModalWindowOnTop()->DrawToolTips(prevDrawToolTips);
			SDL_SetRenderTarget(sdlRenderer, prevRenderTarget);
		}
		if (!modalWindows.empty())
		{
			SynchroniseGainableStates(ModalWindowOnTop().get(), true);
		}
		modalWindows.push_back(ModalWindowEntry{ mw, backdrop, Ticks() });
		{
			Event ev;
			ev.type = Event::RENDERERUP;
			ev.renderer.renderer = sdlRenderer;
			mw->HandleEvent(ev);
		}
		SynchroniseGainableStates(mw.get(), false);
		discardTextInput = true;
	}

	void SDLWindow::PopBack(ModalWindow *mw)
	{
		if (modalWindows.back().window.get() != mw)
		{
			throw std::runtime_error("attempt to pop window not on top of the stack");
		}
		SynchroniseGainableStates(mw, true);
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
			SynchroniseGainableStates(ModalWindowOnTop().get(), false);
		}
		discardTextInput = true;
	}

	void SDLWindow::SynchroniseGainableStates(ModalWindow *mw, bool spoofAllFalse)
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
		frameStart = Ticks();
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
			UpdateModState();
			ev.key.shift = modShift;
			ev.key.ctrl = modCtrl;
			ev.key.alt = modAlt;
			mw->HandleEvent(ev);
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			mouseButtonsDown += sdlEvent.type == SDL_MOUSEBUTTONDOWN ? 1 : -1;
#ifndef DEBUG
			SDL_CaptureMouse(mouseButtonsDown > 0 ? SDL_TRUE : SDL_FALSE);
#endif
			ev.type = sdlEvent.type == SDL_MOUSEBUTTONDOWN ? Event::MOUSEDOWN : Event::MOUSEUP;
			ev.mouse.button = sdlEvent.button.button;
			ev.mouse.clicks = sdlEvent.button.clicks;
			mw->HandleEvent(ev);
			break;

		case SDL_MOUSEMOTION:
			mousePosition = Point{ sdlEvent.motion.x, sdlEvent.motion.y };
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
			case SDL_WINDOWEVENT_ENTER:
				underMouse = true;
				{
					Point global, window;
					SDL_GetGlobalMouseState(&global.x, &global.y);
					SDL_GetWindowPosition(sdlWindow, &window.x, &window.y);
					mousePosition = (global - window) / conf.scale;
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

	void SDLWindow::DrawTopWindow()
	{
		int32_t pushedFor = Ticks() - modalWindows.back().pushedAt;
		if (pushedFor > backdropDarkenTicks)
		{
			pushedFor = backdropDarkenTicks;
		}
		DrawWindowWithBackdrop(ModalWindowOnTop().get(), modalWindows.back().backdrop, backdropDarkenAlpha * pushedFor / backdropDarkenTicks);
	}

	void SDLWindow::DrawWindowWithBackdrop(const ModalWindow *mw, SDL_Texture *backdrop, int alpha)
	{
		if (backdrop)
		{
			SDL_RenderCopy(sdlRenderer, backdrop, NULL, NULL);
			SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, alpha);
			SDL_RenderFillRect(sdlRenderer, NULL);
		}
		else
		{
			SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
			SDL_RenderClear(sdlRenderer);
		}
		Event ev;
		ev.type = Event::DRAW;
		mw->HandleEvent(ev);
	}

	void SDLWindow::FrameTick()
	{
		TickTopWindow();
	}

	void SDLWindow::FrameDraw()
	{
		int rpsLimit = ModalWindowOnTop()->RPSLimit();
		if (!rpsLimit || timeSinceRender > 1000.f / rpsLimit)
		{
			timeSinceRender = 0;
			DrawTopWindow();
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
		if (SDL_RenderSetClipRect(sdlRenderer, effective))
		{
			throw std::runtime_error(ByteString("SDL_RenderSetClipRect failed\n") + SDL_GetError());
		}
	}

	void SDLWindow::DrawLine(Point from, Point to, Color color)
	{
		SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
		SDL_RenderDrawLine(sdlRenderer, from.x, from.y, to.x, to.y);
	}
	
	void SDLWindow::DrawRect(Rect rect, Color color)
	{
		SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
		SDL_Rect sdlRect;
		sdlRect.x = rect.pos.x;
		sdlRect.y = rect.pos.y;
		sdlRect.w = rect.size.x;
		sdlRect.h = rect.size.y;
		SDL_RenderFillRect(sdlRenderer, &sdlRect);
	}

	void SDLWindow::DrawRectOutline(Rect rect, Color color)
	{
		SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
		SDL_Rect sdlRect;
		sdlRect.x = rect.pos.x;
		sdlRect.y = rect.pos.y;
		sdlRect.w = rect.size.x;
		sdlRect.h = rect.size.y;
		SDL_RenderDrawRect(sdlRenderer, &sdlRect);
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
		static_assert(textureSize <= 28, "textureSize too big");
		uint32_t textureWidth = 1 << (textureSize - textureSize / 2);
		uint32_t textureHeight = 1 << (textureSize / 2);
		fontSurface = SDL_CreateRGBSurfaceWithFormat(0, textureWidth, textureHeight, 32, SDL_PIXELFORMAT_ARGB8888);
		auto dataIt = fontGlyphData.begin();
		auto *ptrs = fd.ptrs;
		uint32_t atX = 0;
		uint32_t atY = 0;
		SDL_LockSurface(fontSurface);
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
					// * If you get this, increase textureSize above.
					throw std::runtime_error("textureSize too small");
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
						reinterpret_cast<uint32_t *>(&pixels[(atY + y) * fontSurface->pitch])[atX + x] = ((buffer & 3) * 0x55000000U) | 0xFFFFFFU;
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
		SDL_SetTextureBlendMode(fontTexture, SDL_BLENDMODE_BLEND);
		SDL_SetTextureAlphaMod(fontTexture, initial.a);
		Point drawAt = position;
		Point nextOffset = Point{ 0, 0 };
		bool canBacktrack = false;
		bool bold   = flags & drawTextBold;
		bool invert = flags & drawTextInvert;
		bool darken = flags & drawTextDarken;
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
			SDL_SetTextureColorMod(fontTexture, newColor.r, newColor.g, newColor.b);
		};
		setColor(initial);
		int backtrackBy = 0;
		int monospace = (flags & drawTextMonospaceMask) >> drawTextMonospaceShift;
		for (auto it = text.begin(); it != text.end(); ++it)
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

				case 'a':
					if (!invert)
					{
						SDL_SetTextureBlendMode(fontTexture, SDL_BLENDMODE_ADD);
					}
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
					if (escape >= 0x100)
					{
					}
					else if (escape >= 0xC0)
					{
						nextOffset.x = escape & 7;
						nextOffset.y = (escape & 56) >> 3;
						if (nextOffset.x & 4) nextOffset.x |= ~7;
						if (nextOffset.y & 4) nextOffset.y |= ~7;
					}
					else if (escape >= 0xB0)
					{
						drawAt.x += escape & 15;
					}
					else if (escape >= 0xA0)
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
				setColor(Color{ int(r), int(g), int(b) });
				canBacktrack = false;
			}
			else
			{
				uint32_t gd = GlyphData(ch);
				SDL_Rect rectFrom, rectTo;
				rectFrom.x = (gd & 0x0003FFF0U) >> 4;
				rectFrom.y = (gd & 0xFFFC0000U) >> 18;
				rectFrom.w = gd & 0x0000000FU;
				rectFrom.h = FONT_H;
				rectTo.x = drawAt.x + nextOffset.x;
				rectTo.y = drawAt.y + nextOffset.y;
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
				SDL_RenderCopy(sdlRenderer, fontTexture, &rectFrom, &rectTo);
				if (bold)
				{
					rectTo.x += 1;
					SDL_RenderCopy(sdlRenderer, fontTexture, &rectFrom, &rectTo);
				}
#if 0
				DrawRectOutline(Rect{ Point{ rectTo.x, rectTo.y }, Point{ rectTo.w, rectTo.h } }, Color{ 0, 255, 0, 192 });
#endif
				canBacktrack = true;
				backtrackBy = monospace ? monospace : rectFrom.w;
				nextOffset = Point{ 0, 0 };
			}
		}
	}

	Point SDLWindow::TextSize(const String &text, uint32_t flags) const
	{
		int boxWidth = 0;
		int boxHeight = FONT_H;
		int lineWidth = 0;
		bool canBacktrack = false;
		int backtrackBy = 0;
		int monospace = (flags & drawTextMonospaceMask) >> drawTextMonospaceShift;
		for (auto it = text.begin(); it != text.end(); ++it)
		{
			auto ch = *it;
			if (ch == '\n')
			{
				lineWidth = 0;
				boxHeight += FONT_H;
				canBacktrack = false;
			}
			else if (ch == '\b' && it + 1 < text.end())
			{
				auto escape = *(++it);
				switch (escape)
				{
				case 'x':
					if (canBacktrack)
					{
						lineWidth -= backtrackBy;
					}
					break;

				default:
					if (escape >= 0x100)
					{
					}
					else if (escape >= 0xC0)
					{
					}
					else if (escape >= 0xB0)
					{
						lineWidth += escape & 15;
						if (boxWidth < lineWidth)
						{
							boxWidth = lineWidth;
						}
					}
					else if (escape >= 0xA0)
					{
						monospace = escape & 15;
					}
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
				uint32_t charWidth = GlyphData(ch) & 0x0000000FU;
				lineWidth += monospace ? monospace : charWidth;
				if (boxWidth < lineWidth)
				{
					boxWidth = lineWidth;
				}
				canBacktrack = true;
				backtrackBy = monospace ? monospace : charWidth;
			}
		}
		return Point{ boxWidth, boxHeight };
	}

	void SDLWindow::ClipboardText(const ByteString &text)
	{
		SDL_SetClipboardText(text.c_str());
	}

	ByteString SDLWindow::ClipboardText()
	{
		return ByteString(SDL_GetClipboardText());
	}

	bool SDLWindow::ClipboardEmpty()
	{
		return !SDL_HasClipboardText();
	}

	int32_t SDLWindow::Ticks()
	{
		return int32_t(SDL_GetTicks());
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
		sdlRect.x = rect.pos.x * conf.scale;
		sdlRect.y = rect.pos.y * conf.scale;
		sdlRect.w = rect.size.x * conf.scale;
		sdlRect.h = rect.size.y * conf.scale;
		SDL_SetTextInputRect(&sdlRect);
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
		// * TODO-REDO_UI: make it possible to remap these
		if (modShift) modState |= mod0;
		if (modCtrl ) modState |= mod1;
		if (modAlt  ) modState |= mod2;
	}

	void SDLWindow::BlueScreen(String details)
	{
		DrawRect(Rect{ Point{ 0, 0 }, Configuration().size }, Color{ 17, 114, 169, 210 });
		StringBuilder sb;
		sb << "ERROR\n\n";
		sb << "Details: " << details << "\n\n";
		sb << "An unrecoverable fault has occurred, please report the error by visiting the website below\n";
		sb << SCHEME SERVER;
		String message = sb.Build();
		DrawText((Configuration().size - TextSize(message)) / 2, message, Color{ 255, 255, 255, 255 });
		FrameEnd();
		while (true)
		{
			SDL_Event sdlEvent;
			while (SDL_PollEvent(&sdlEvent))
			{
				if (sdlEvent.type == SDL_QUIT)
				{
#ifdef MACOSX
					exit(-1); // * apple pls
#else
					quick_exit(-1);
#endif
				}
			}
			SDL_Delay(20);
		}
	}

}
