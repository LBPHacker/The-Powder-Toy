#pragma once
#include "Common/Color.hpp"
#include "Common/ExplicitSingleton.hpp"
#include "Common/NoCopy.hpp"
#include "Common/Point.hpp"
#include "Common/Rect.hpp"
#include "common/tpt-rand.h"
#include "Ticks.hpp"
#include "Text.hpp"
#include "TextWrapper.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_set>

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef union SDL_Event SDL_Event;
typedef struct SDL_Texture SDL_Texture;

#define DebugGuiHost 0

namespace Powder::Gui
{
	class ViewStack;

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
		SDL_Texture *texture = nullptr;
		bool haveTexture = false;
	};

	class Host : public NoCopy, public ExplicitSingleton<Host>
	{
		SDL_Window *sdlWindow = nullptr;
		SDL_Renderer *sdlRenderer = nullptr;
		bool explicitDelay = false;

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
		struct ViewStackHolder
		{
			Host &host;
			std::shared_ptr<ViewStack> viewStack;

			ViewStackHolder(Host &newHost, std::shared_ptr<ViewStack> newViewStack);
			~ViewStackHolder();

			void SetHost(Host *newHost);
			void SetRendererUp(bool newRendererUp);
		};
		std::shared_ptr<ViewStackHolder> viewStackHolder;

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

	public:
		struct WindowParameters
		{
			int32_t testValue = 0;

			bool operator ==(const WindowParameters &) const = default;
		};

	private:
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
			Host &host;
			std::unordered_set<ShapedTextIndex, ShapedTextHash, ShapedTextEqual> indexSet;

		public:
			ShapedTextStore(Host &newHost);

			ShapedTextIndex Alloc(ShapeTextParameters stp);
			void Free(CrossFrameItem::Index index);
			std::optional<ShapedTextIndex> GetIndex(ShapeTextParameters stp);
		};
		ShapedTextStore shapedTextStore{ *this };

		void CreateShapedTextTexture(ShapedText &shapedText);
		void DestroyShapedTextTexture(ShapedText &shapedText);

		Ticks lastTick = 0;
		WindowParameters windowParameters;
		void Open();
		void Close();

		Rect clipRect{ { 0, 0 }, { 0, 0 } };

		RNG rng;
		bool momentumScroll = true;

	public:
		Host();
		~Host();

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

		void SetWindowParameters(WindowParameters newWindowParameters);

		void SetMomentumScroll(bool newMomentumScroll)
		{
			momentumScroll = newMomentumScroll;
		}
		bool GetMomentumScroll() const
		{
			return momentumScroll;
		}

		void Start();
		void Stop();
		bool IsRunning() const;
		void HandleFrame();
		void SetViewStack(std::shared_ptr<ViewStack> newViewStack);
		void SetClipRect(Rect newClipRect);
		Rect GetClipRect() const;

		void DrawRect(Rect rect, Rgba8 color);
		void FillRect(Rect rect, Rgba8 color);

		void DrawPoint(Point pos, Rgba8 color);
		void DrawLine(Point from, Point to, Rgba8 color);

		void DrawText(Point pos, ShapedTextIndex text, Rgba8 color);
		void DrawText(Point pos, InternedTextIndex text, Rgba8 color);
		void DrawText(Point pos, std::string_view text, Rgba8 color);

		void StartTextInput();
		void StopTextInput();

		Ticks GetLastTick() const
		{
			return lastTick;
		}

		Point GetSize() const;

		RNG &GetRng()
		{
			return rng;
		}

		void CopyTextToClipboard(std::string text);
		std::optional<std::string> PasteTextFromClipboard();
	};
}
