#pragma once

#include "common/ExplicitSingleton.h"
#include "common/String.h"
#include "Rect.h"
#include "Color.h"

#include <tuple>
#include <cstdint>
#include <list>
#include <memory>

union SDL_Event;
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Surface;

namespace gui
{
	class ModalWindow;

	inline String IconString(int icon, Point off)
	{
		return String(0x08) + String(0xC0 | ((off.y & 7) << 3) | (off.x & 7)) + String(icon);
	}

	inline String ColorString(gui::Color color)
	{
		return String(0x0F) + String(color.r) + String(color.g) + String(color.b);
	}

	enum CommonColor
	{
		commonWhite,
		commonLightGray,
		commonYellow,
		commonRed,
		commonLightRed,
		commonBlue,
		commonLightBlue,
		commonPurple,
	};
	inline String CommonColorString(CommonColor commonColor)
	{
		switch (commonColor)
		{
		case commonWhite    : return String(0x08) + String('w');
		case commonLightGray: return String(0x08) + String('g');
		case commonYellow   : return String(0x08) + String('o');
		case commonRed      : return String(0x08) + String('r');
		case commonLightRed : return String(0x08) + String('l');
		case commonBlue     : return String(0x08) + String('b');
		case commonLightBlue: return String(0x08) + String('t');
		case commonPurple   : return String(0x08) + String('u');
		}
		return String();
	}

	inline String OffsetString(int offset)
	{
		return String(0x08) + String(0xB0 + (offset & 15));
	}

	inline String BlendAddString()
	{
		return String(0x08) + String("a");
	}

	inline String InvertString()
	{
		return String(0x08) + String("i");
	}

	inline String StepBackString()
	{
		return String(0x08) + String("x");
	}

	class SDLWindow : public ExplicitSingleton<SDLWindow>
	{
	public:
		struct Configuration
		{
			Point size;
			int scale;
			bool fullscreen;
			bool borderless;
			bool resizable;
			bool integer;
		};

		static constexpr int mod0 = 0x01;
		static constexpr int mod1 = 0x02;
		static constexpr int mod2 = 0x04;
		static constexpr int mod3 = 0x08;
		static constexpr int mod4 = 0x10;
		static constexpr int mod5 = 0x20;
		static constexpr int mod6 = 0x40;
		static constexpr int mod7 = 0x80;
		static constexpr int modBits = 8;

	private:
		struct ModalWindowEntry
		{
			std::shared_ptr<ModalWindow> window;
			SDL_Texture *backdrop;
			int32_t pushedAt;
		};

		SDL_Window *sdlWindow = NULL;
		SDL_Renderer *sdlRenderer = NULL;
		SDL_Texture *fontTexture = NULL;
		SDL_Surface *fontSurface = NULL;

		void AssertModalWindowsEmpty();
		void Close();
		void ApplyClipRect();

		std::list<ModalWindowEntry> modalWindows;

		Rect clipRect;

		bool modShift = false;
		bool modCtrl = false;
		bool modAlt = false;

		void UpdateModState();
		int modState = 0;

		Configuration conf;
		Point mousePosition = Point{ 0, 0 };
		int32_t frameStart = 0;
		int32_t lastFpsUpdate = 0;
		int32_t prevFrameStart = 0;
		float frameTimeAvg = 0.f;
		float correctedFrameTimeAvg = 0.f;
		float fpsMeasured = 0.f;
		float timeSinceRender = 0.f;
		bool underMouse = false;
		int mouseButtonsDown = 0;
		bool withFocus = false;
		bool momentumScroll = false;
		bool fastQuit = false;

		void InitFontSurface();
		uint32_t GlyphData(int ch) const;

		std::vector<uint32_t> fontGlyphData;
		std::vector<std::tuple<int, int>> fontGlyphRanges;

		SDLWindow(const SDLWindow &) = delete;
		SDLWindow &operator =(const SDLWindow &) = delete;

		void TickTopWindow();
		void DrawTopWindow();
		void DrawWindowWithBackdrop(const ModalWindow *mw, SDL_Texture *backdrop, int alpha);

		void SynchroniseGainableStates(ModalWindow *mw, bool spoofAllFalse);

		bool textInput = false;
		String textEditingBuffer;

		bool eglUnloadWorkaround = false;
		bool discardTextInput = false;

	public:
		SDLWindow(Configuration conf);
		~SDLWindow();

		void PushBack(std::shared_ptr<ModalWindow> mw);
		void PopBack(ModalWindow *mw);

		template<class ModalWindowType, class ...Args>
		std::shared_ptr<ModalWindowType> EmplaceBack(Args &&...args)
		{
			auto ptr = std::make_shared<ModalWindowType>(std::forward<Args>(args)...);
			PushBack(ptr);
			return ptr;
		}

		bool Running() const
		{
			return sdlWindow && !modalWindows.empty();
		}

		std::shared_ptr<ModalWindow> ModalWindowOnTop() const
		{
			return modalWindows.empty() ? nullptr : modalWindows.back().window;
		}

		void FrameBegin();
		void FramePoll();
		void FrameEvent(const SDL_Event &sdlEvent);
		void FrameTick();
		void FrameDraw();
		void FrameEnd();
		void FrameDelay();
		void FrameTime();

		void ClipRect(Rect newClipRect);
		Rect ClipRect() const
		{
			return clipRect;
		}

		void DrawLine(Point from, Point to, Color color);
		void DrawRect(Rect rect, Color color);
		void DrawRectOutline(Rect rect, Color color);

		static constexpr uint32_t drawTextBold           =    1U;
		static constexpr uint32_t drawTextInvert         =    2U;
		static constexpr uint32_t drawTextDarken         =    4U;
		static constexpr uint32_t drawTextMonospaceShift =    4U;
		static constexpr uint32_t drawTextMonospaceMask  = 0xF0U;
		static uint32_t DrawTextMonospace(int spacing)
		{
			return ((uint32_t(spacing)) << drawTextMonospaceShift) & drawTextMonospaceMask;
		};
		void DrawText(Point position, const String &text, Color color, uint32_t flags = 0U);
		Point TextSize(const String &text, uint32_t flags = 0U) const;
		int CharWidth(int ch) const;

		double GetFPS() const
		{
			return fpsMeasured;
		}

		Point MousePosition() const
		{
			return mousePosition;
		}

		bool UnderMouse() const
		{
			return underMouse;
		}

		static void ClipboardText(const ByteString &text);
		static ByteString ClipboardText();
		static bool ClipboardEmpty();
		static int32_t Ticks();

		void Recreate(Configuration newConf);
		const Configuration &Config() const
		{
			return conf;
		}

		bool ModShift() const
		{
			return modShift;
		}

		bool ModCtrl() const
		{
			return modCtrl;
		}

		bool ModAlt() const
		{
			return modAlt;
		}

		int Mod() const
		{
			return modState;
		}

		void MomentumScroll(bool newMomentumScroll)
		{
			momentumScroll = newMomentumScroll;
		}
		bool MomentumScroll() const
		{
			return momentumScroll;
		}

		void FastQuit(bool newFastQuit)
		{
			fastQuit = newFastQuit;
		}
		bool FastQuit() const
		{
			return fastQuit;
		}

		int MaxScale() const;
		void StartTextInput();
		void StopTextInput();
		void TextInputRect(Rect rect);

		static ByteString GetPrefPath();
		void BlueScreen(String details);
	};
}
