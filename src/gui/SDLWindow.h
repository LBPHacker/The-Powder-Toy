#pragma once
#include "Config.h"

#include "common/ExplicitSingleton.h"
#include "common/String.h"
#include "Rect.h"
#include "Color.h"
#include "Cursor.h"

#include <array>
#include <tuple>
#include <cstdint>
#include <list>
#include <memory>

#ifdef DrawText
# undef DrawText // thank you windows
#endif

union SDL_Event;
struct SDL_Cursor;
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Surface;

namespace gui
{
	class ModalWindow;
	class Texture;

	inline String AlignString(Point off)
	{
		return String(0x08) + String(0xC0 | ((off.y & 7) << 3) | (off.x & 7));
	}

	inline String IconString(int icon, Point off)
	{
		return AlignString(off) + String(icon);
	}

	inline String ColorString(Color color)
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
		return String(0x08) + String(0x100 | (offset & 0xFF));
	}

	inline String ResetString()
	{
		return String(0x08) + String("z");
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

	class SDLWindow : public common::ExplicitSingleton<SDLWindow>
	{
	public:
		class NarrowClipRect
		{
			Rect prevClipRect;

			NarrowClipRect(const NarrowClipRect &) = delete;
			NarrowClipRect &operator =(const NarrowClipRect &) = delete;

		public:
			NarrowClipRect(Rect newClipRect)
			{
				prevClipRect = SDLWindow::Ref().ClipRect();
				SDLWindow::Ref().ClipRect(prevClipRect.Intersect(newClipRect));
			}

			~NarrowClipRect()
			{
				SDLWindow::Ref().ClipRect(prevClipRect);
			}
		};

		struct Configuration
		{
			Point size;
			int scale;
			bool fullscreen;
			bool borderless;
			bool resizable;
			bool blurry;
			bool integer;
			enum RendererType
			{
				rendererDefault,
				rendererSoftware,
				rendererAccelerated,
				rendererMax, // * Must be the last enum constant.
			};
			RendererType rendererType;
		};

		static constexpr uint32_t mod0 = UINT32_C(0x00000001);
		static constexpr uint32_t mod1 = UINT32_C(0x00000002);
		static constexpr uint32_t mod2 = UINT32_C(0x00000004);
		static constexpr uint32_t mod3 = UINT32_C(0x00000008);
		static constexpr uint32_t mod4 = UINT32_C(0x00000010);
		static constexpr uint32_t mod5 = UINT32_C(0x00000020);
		static constexpr uint32_t mod6 = UINT32_C(0x00000040);
		static constexpr uint32_t mod7 = UINT32_C(0x00000080);
		static constexpr int modBits = 8;

		static constexpr uint32_t forbidGenerated = UINT32_C(0x00000001);
		static constexpr uint32_t forbidTreeOp    = UINT32_C(0x00000002);
		class ForbidMethods
		{
			uint32_t prevForbiddenMethods;

		public:
			ForbidMethods(uint32_t newForbiddenMethods);
			~ForbidMethods();
		};

	private:
		struct ModalWindowEntry
		{
			std::shared_ptr<ModalWindow> window;
			SDL_Texture *backdrop;
			int32_t pushedAt;
		};

		SDL_Window *sdlWindow = NULL;
		SDL_Renderer *sdlRenderer = NULL;
		SDL_Texture *topLevelTexture = NULL;
		SDL_Texture *fontTexture = NULL;
		SDL_Surface *fontSurface = NULL;

		void AssertModalWindowsEmpty();
		void Close();
		void ApplyClipRect();

		std::list<ModalWindowEntry> modalWindows;

		Rect clipRect;

		uint32_t forbiddenMethods = 0;

		bool modShift = false;
		bool modCtrl = false;
		bool modAlt = false;

		Cursor::CursorType lastHoverCursor = Cursor::cursorArrow;
		std::array<SDL_Cursor *, Cursor::cursorMax> hoverCursors;
		bool shouldUpdateHoverCursor = true;
		void UpdateHoverCursorIfNeeded();

		void UpdateModState();
		uint32_t modState = 0;

		Configuration conf;
		Point mousePosition = { 0, 0 };
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

		void TickTopWindow();
		void DrawTopWindow();
		void DrawWindowWithBackdrop(const ModalWindowEntry &mwe);

		void UpdateGainableStates(ModalWindow *mw, bool spoofAllFalse);

		bool textInput = false;
		String textEditingBuffer;

		bool eglUnloadWorkaround = false;
		bool discardTextInput = false;

		String timeFormat = "%c";

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
		void DrawTexture(const Texture &texture, Rect rect);
		void DrawTexture(const Texture &texture, Rect rect, Rect source);

		static constexpr uint32_t drawTextBold           = 0x0001U;
		static constexpr uint32_t drawTextInvert         = 0x0002U;
		static constexpr uint32_t drawTextDarken         = 0x0004U;
		static constexpr uint32_t drawTextUnderline      = 0x0008U;
		static constexpr uint32_t drawTextMonospaceMask  = 0x00F0U;
		static constexpr uint32_t drawTextShaded         = 0x0100U;
		static constexpr unsigned int drawTextMonospaceShift = 4;
		static uint32_t DrawTextMonospace(int spacing)
		{
			return ((uint32_t(spacing)) << drawTextMonospaceShift) & drawTextMonospaceMask;
		};
		void DrawText(Point position, const String &text, Color color, uint32_t flags = 0U);
		Point TextSize(const String &text, uint32_t flags = 0U) const;
		int FitText(Point &size, const String &text, int limit, uint32_t flags = 0U) const;
		int TruncateText(String &truncated, Point &size, const String &text, int limit, const String &overflowText, uint32_t flags = 0U) const;
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

		static void ClipboardText(const String &text);
		static String ClipboardText();
		static bool ClipboardEmpty();

		int32_t Ticks() const;

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

		uint32_t Mod() const
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

		void AssertAllowed(uint32_t methodsToCheck);
		friend class ForbidMethods;

		void TimeFormat(String newTimeFormat)
		{
			timeFormat = newTimeFormat;
		}
		const String &TimeFormat() const
		{
			return timeFormat;
		}

		void ShouldUpdateHoverCursor()
		{
			shouldUpdateHoverCursor = true;
		}

		gui::Rect RendererRect() const;

#ifdef DEBUG
		bool nudgingComponents = false;
		bool drawComponentRects = false;
		bool drawContentRects = false;
		bool drawCharacterRects = false;
#endif
	};
}
