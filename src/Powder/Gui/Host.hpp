#pragma once
#include "Common/Color.hpp"
#include "Common/NoCopy.hpp"
#include "Common/Point.hpp"
#include "Common/Rect.hpp"
#include "common/tpt-rand.h"
#include "Text.hpp"
#include "TextWrapper.hpp"
#include "Ticks.hpp"
#include "CommonMetrics.hpp"
#include "WindowParameters.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_set>
#include <variant>
#include <list>

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef union SDL_Event SDL_Event;
typedef struct SDL_Texture SDL_Texture;

#define DebugGuiHost 0

namespace Powder::Gui
{
	class Stack;
	class StaticTexture;

	struct InternedTextStoreSpan
	{
		int32_t base, size;
	};

	struct InternedText : public CrossFrameItem
	{
		InternedTextStoreSpan text;
	};

	struct ShapedText : public CrossFrameItem
	{
		ShapeTextParameters stp;
		TextWrapper wrapper;
		std::unique_ptr<StaticTexture> texture;
	};

	class Host : public NoCopy
	{
		SDL_Window *sdlWindow = nullptr;
		SDL_Renderer *sdlRenderer = nullptr;
		SDL_Texture *sdlTexture = nullptr;
		bool explicitDelay = false;

		std::list<StaticTexture *> staticTextures;
		void CreateStaticTextureTexture(StaticTexture &staticTexture);
		void DestroyStaticTextureTexture(StaticTexture &staticTexture);

		std::vector<char> fontData;
		std::vector<uint32_t> fontPtrs;
		struct FontRange
		{
			uint32_t first;
			uint32_t last;
			int32_t ptrBase;
		};
		std::vector<FontRange> fontRanges;
		void LoadFontData();

		bool running = false;
		struct StackHolder
		{
			Host &host;
			std::shared_ptr<Stack> stack;

			StackHolder(Host &newHost, std::shared_ptr<Stack> newStack);
			~StackHolder();

			void SetRendererUp(bool newRendererUp);
			void SetOnTop(bool newOnTop);
		};
		std::shared_ptr<StackHolder> stackHolder;

		class InternedTextStore;
		struct InternedTextHash
		{
			using is_transparent = void;

			InternedTextStore *store;

			size_t operator ()(const InternedTextIndex &index) const;
			size_t operator ()(std::string_view view) const;
		};
		struct InternedTextEqual
		{
			using is_transparent = void;

			InternedTextStore *store;

			bool operator ()(const InternedTextIndex &lhs, const InternedTextIndex &rhs) const;
			bool operator ()(const InternedTextIndex &lhs, std::string_view rhs) const;
			bool operator ()(std::string_view lhs, const InternedTextIndex &rhs) const;
		};

		class InternedTextStore : public CrossFrameItemStore<InternedTextStore, InternedText>
		{
#if DebugGuiHost
		public:
#endif
			std::vector<char> textData;
			std::unordered_set<InternedTextIndex, InternedTextHash, InternedTextEqual> indexSet;

			void PruneText();

		public:
			InternedTextStore();
			InternedTextStoreSpan InternText(std::string_view text);

			std::string_view DereferenceSpan(InternedTextStoreSpan span) const;

			InternedTextIndex Alloc(std::string_view text);
			void Free(CrossFrameItem::Index index);
			std::optional<InternedTextIndex> GetIndex(std::string_view text);

			bool EndFrame();

			friend struct InternedTextHash;
		};
		InternedTextStore internedTextStore;

		class ShapedTextStore;
		struct ShapedTextHash
		{
			using is_transparent = void;

			ShapedTextStore *store;

			size_t operator ()(const ShapedTextIndex &index) const;
			size_t operator ()(ShapeTextParameters stp) const;
		};
		struct ShapedTextEqual
		{
			using is_transparent = void;

			ShapedTextStore *store;

			bool operator ()(const ShapedTextIndex &lhs, const ShapedTextIndex &rhs) const;
			bool operator ()(const ShapedTextIndex &lhs, ShapeTextParameters rhs) const;
			bool operator ()(ShapeTextParameters lhs, const ShapedTextIndex &rhs) const;
		};

		class ShapedTextStore : public CrossFrameItemStore<ShapedTextStore, ShapedText>
		{
			Host &host; // TODO-REDO_UI: turn into a base
			std::unordered_set<ShapedTextIndex, ShapedTextHash, ShapedTextEqual> indexSet;

		public:
			ShapedTextStore(Host &newHost);

			ShapedTextIndex Alloc(ShapeTextParameters stp);
			void Free(CrossFrameItem::Index index);
			std::optional<ShapedTextIndex> GetIndex(ShapeTextParameters stp);
		};
		ShapedTextStore shapedTextStore{ *this };

		void CreateShapedTextTexture(ShapedText &shapedText);

		Ticks lastTick = 0;
		WindowParameters windowParameters;
		void CreateWindow();
		void DestroyWindow();
		void CreateRenderer();
		void DestroyRenderer();
		void Open();
		void Close();

		Rect clipRect{ { 0, 0 }, { 0, 0 } };

		RNG rng;
		bool momentumScroll = true;
		bool showAvatars = true;
		bool fastQuit = true;
		bool globalQuit = false;

		CommonMetrics commonMetrics;

	public:
		Host();
		~Host();

		SDL_Window *GetWindow() const
		{
			return sdlWindow;
		}

		SDL_Renderer *GetRenderer() const
		{
			return sdlRenderer;
		}

		InternedTextIndex InternText(std::string_view text);
		std::string_view GetInternedText(InternedTextIndex index) const;

		ShapedTextIndex ShapeText(InternedTextIndex text, std::optional<int32_t> maxWidth = {});
		ShapedTextIndex ShapeText(ShapeTextParameters stp);
		const TextWrapper &GetShapedTextWrapper(ShapedTextIndex index) const;

		const uint8_t *CharData(uint32_t ch) const;
		int32_t CharWidth(uint32_t ch) const;

		void SetWindowParameters(WindowParameters newWindowParameters)
		{
			windowParameters = newWindowParameters;
		}
		const WindowParameters &GetWindowParameters() const
		{
			return windowParameters;
		}

		Point GetDesktopSize() const;

		void SetMomentumScroll(bool newMomentumScroll)
		{
			momentumScroll = newMomentumScroll;
		}
		bool GetMomentumScroll() const
		{
			return momentumScroll;
		}

		void SetShowAvatars(bool newShowAvatars)
		{
			showAvatars = newShowAvatars;
		}
		bool GetShowAvatars() const
		{
			return showAvatars;
		}

		void SetFastQuit(bool newFastQuit)
		{
			fastQuit = newFastQuit;
		}
		bool GetFastQuit() const
		{
			return fastQuit;
		}

		void SetGlobalQuit(bool newGlobalQuit)
		{
			globalQuit = newGlobalQuit;
		}
		bool GetGlobalQuit() const
		{
			return globalQuit;
		}

		void Start();
		void Stop();
		bool IsRunning() const;
		void HandleFrame();
		void SetStack(std::shared_ptr<Stack> newStack);
		void SetClipRect(Rect newClipRect);
		Rect GetClipRect() const;

		void DrawRect(Rect rect, Rgba8 color);
		void FillRect(Rect rect, Rgba8 color);

		void DrawPoint(Point pos, Rgba8 color);
		void DrawLine(Point from, Point to, Rgba8 color);

		void DrawText(Point pos, ShapedTextIndex text, Rgba8 color);
		void DrawText(Point pos, InternedTextIndex text, Rgba8 color);
		void DrawText(Point pos, std::string_view text, Rgba8 color);

		void DrawStaticTexture(Rect from, Rect to, StaticTexture &staticTexture, Rgba8 color);
		void DrawStaticTexture(Rect to, StaticTexture &staticTexture, Rgba8 color);
		void DrawStaticTexture(Point to, StaticTexture &staticTexture, Rgba8 color);

		void StartTextInput();
		void StopTextInput();

		Ticks GetLastTick() const
		{
			return lastTick;
		}

		Point GetSize() const
		{
			return windowParameters.windowSize;
		}

		RNG &GetRng()
		{
			return rng;
		}

		void CopyTextToClipboard(std::string text);
		std::optional<std::string> PasteTextFromClipboard();

		void AddStaticTexture(StaticTexture &st);
		void RemoveStaticTexture(StaticTexture &st);

		const CommonMetrics &GetCommonMetrics() const
		{
			return commonMetrics;
		}
	};
}
