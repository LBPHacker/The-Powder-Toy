#include "SdlAssert.hpp"
#include "Config.h"
#include "Common/Bz2.hpp"
#include "Common/HandlePtr.hpp"
#include "Common/NoCopy.hpp"
#include "Common/Utf8.hpp"
#include "Gui/Icons.hpp"
#include "StaticTexture.hpp"
#include "FontBz2.h"
#include "Host.hpp"
#include "Stack.hpp"
#include "Colors.hpp"
#include "graphics/VideoBuffer.h"
#include "WindowIcon.h"
#include <algorithm>

#if DebugGuiHost
# include "Common/Log.hpp"
#endif

namespace
{
	template<class T>
	inline void HashCombine(size_t &s, const T &v)
	{
		std::hash<T> h;
		s ^= h(v) + 0x9E3779B9 + (s << 6) + (s >> 2);
	}
}

std::size_t std::hash<Powder::Gui::ShapeTextParameters>::operator ()(const Powder::Gui::ShapeTextParameters &stp) const
{
	size_t h = 0;
	HashCombine(h, stp.text.value);
	HashCombine(h, stp.maxWidth);
	HashCombine(h, stp.alignment);
	HashCombine(h, stp.firstLineIndent);
	HashCombine(h, stp.overflowText ? stp.overflowText->value : std::optional<int32_t>{});
	HashCombine(h, stp.replaceWithDots);
	return h;
}

template<>
struct std::hash<Powder::Gui::ShapeTextParameters::MaxWidth>
{
	std::size_t operator ()(const Powder::Gui::ShapeTextParameters::MaxWidth &maxWidth) const
	{
		size_t h = 0;
		HashCombine(h, maxWidth.width);
		HashCombine(h, maxWidth.fill);
		return h;
	}
};

namespace Powder::Gui
{
	namespace
	{
		constexpr int32_t maxInternedItemSize  = 100'000;
		constexpr int32_t maxInternedTotalSize = 10'000'000;
		constexpr int32_t explicitDelayMs      = 16;
		constexpr int32_t maxFrameRepeatCount  = 10;
	}

	size_t Host::InternedTextHash::operator ()(const InternedTextIndex &index) const
	{
		return std::hash<std::string_view>{}(store->DereferenceSpan((*store)[index].text));
	}

	size_t Host::InternedTextHash::operator ()(std::string_view view) const
	{
		return std::hash<std::string_view>{}(view);
	}

	bool Host::InternedTextEqual::operator ()(const InternedTextIndex &lhs, const InternedTextIndex &rhs) const
	{
		return lhs == rhs;
	}

	bool Host::InternedTextEqual::operator ()(const InternedTextIndex &lhs, std::string_view rhs) const
	{
		return store->DereferenceSpan((*store)[lhs].text) == rhs;
	}

	bool Host::InternedTextEqual::operator ()(std::string_view lhs, const InternedTextIndex &rhs) const
	{
		return (*this)(rhs, lhs);
	}

	size_t Host::ShapedTextHash::operator ()(const ShapedTextIndex &index) const
	{
		return (*this)((*store)[index].stp);
	}

	size_t Host::ShapedTextHash::operator ()(ShapeTextParameters stp) const
	{
		return std::hash<ShapeTextParameters>()(stp);
	}

	bool Host::ShapedTextEqual::operator ()(const ShapedTextIndex &lhs, const ShapedTextIndex &rhs) const
	{
		return lhs == rhs;
	}

	bool Host::ShapedTextEqual::operator ()(const ShapedTextIndex &lhs, ShapeTextParameters rhs) const
	{
		return (*store)[lhs].stp == rhs;
	}

	bool Host::ShapedTextEqual::operator ()(ShapeTextParameters lhs, const ShapedTextIndex &rhs) const
	{
		return (*this)(rhs, lhs);
	}

	Host::InternedTextStore::InternedTextStore() : indexSet(0, InternedTextHash{ this }, InternedTextEqual{ this })
	{
	}

	InternedTextStoreSpan Host::InternedTextStore::InternText(std::string_view text)
	{
		auto oldKeyStoreSize = int32_t(textData.size());
		auto internedItemSize = std::min(std::min(int32_t(text.size()), maxInternedItemSize), maxInternedTotalSize - oldKeyStoreSize);
		textData.resize(oldKeyStoreSize + internedItemSize);
		std::copy_n(text.data(), internedItemSize, textData.data() + oldKeyStoreSize);
		return { oldKeyStoreSize, internedItemSize };
	}

	void Host::InternedTextStore::PruneText()
	{
		// TODO-REDO_UI: maybe do this only when wasted space ratio exceeds 2 or something
		int32_t newEnd = 0;
		std::vector<char> newTextData(textData.size());
		for (auto &item : *this)
		{
			std::copy_n(textData.data() + item.text.base, item.text.size, newTextData.data() + newEnd);
			item.text.base = newEnd;
			newEnd += item.text.size;
		}
		newTextData.resize(newEnd);
		std::swap(textData, newTextData);
	}

	InternedTextIndex Host::InternedTextStore::Alloc(std::string_view text)
	{
		InternedTextIndex index = { CrossFrameItemStore::Alloc() };
		(*this)[index].text = InternText(text);
		indexSet.insert(index);
		return index;
	}

	void Host::InternedTextStore::Free(CrossFrameItem::Index index)
	{
		indexSet.erase(InternedTextIndex{ index });
	}

	std::optional<InternedTextIndex> Host::InternedTextStore::GetIndex(std::string_view text)
	{
		auto it = indexSet.find(text);
		if (it != indexSet.end())
		{
			return *it;
		}
		return std::nullopt;
	}

	Host::ShapedTextStore::ShapedTextStore(Host &newHost) : host(newHost), indexSet(0, ShapedTextHash{ this }, ShapedTextEqual{ this })
	{
	}

	ShapedTextIndex Host::ShapedTextStore::Alloc(ShapeTextParameters stp)
	{
		ShapedTextIndex index = { CrossFrameItemStore::Alloc() };
		auto &shapedText = (*this)[index];
		shapedText.wrapper.Update(host, stp);
		shapedText.stp = stp;
		indexSet.insert(index);
		return index;
	}

	void Host::ShapedTextStore::Free(CrossFrameItem::Index index)
	{
		auto &shapedText = (*this)[{ index }];
		shapedText.texture.reset();
		shapedText.wrapper = {};
		indexSet.erase(ShapedTextIndex{ index });
	}

	std::optional<ShapedTextIndex> Host::ShapedTextStore::GetIndex(ShapeTextParameters stp)
	{
		auto it = indexSet.find(stp);
		if (it != indexSet.end())
		{
			return *it;
		}
		return std::nullopt;
	}

	void Host::CreateShapedTextTexture(ShapedText &shapedText)
	{
		auto &regions = shapedText.wrapper.GetRegions();
		auto size = shapedText.wrapper.GetShapedSize();
		auto text = GetInternedText(shapedText.stp.text);
		PlaneAdapter<std::vector<pixel>> textureData(size, 0);
		struct Settings
		{
			bool add = false;
			bool underline = false;
			bool invert = false;
			Rgb8 color = 0xFFFFFF_rgb;
			Point offset{ 0, 0 };
		};
		Settings settings;
		auto blend = [&](Point p, Rgba8 color) {
			if (size.OriginRect().Contains(p))
			{
				auto &pixel = textureData[p];
				auto old = Rgba8::Unpack(pixel);
				// TODO-REDO_UI: gamma correct
				if (settings.add)
				{
					pixel = old.Add(color).Pack();
				}
				else
				{
					pixel = old.Blend(color).Pack();
				}
			}
		};
		auto takeRegion = [&](UnicodeCodePoint ch, auto &regionIt) {
			// TODO-REDO_UI: gamma correct
			auto effectiveColor = settings.color;
			if (settings.invert)
			{
				effectiveColor = Rgb8(
					uint8_t(0xFF - effectiveColor.Red  ),
					uint8_t(0xFF - effectiveColor.Green),
					uint8_t(0xFF - effectiveColor.Blue )
				);
			}
			auto *data = CharData(ch);
			data += 1;
			int32_t bitsLeft = 0;
			uint8_t bits = 0;
			auto rect = RectSized(Point{ regionIt->left, regionIt->line * fontTypeSize }, Point{ regionIt->width, fontTypeSize });
			if (settings.offset != Point{ 0, 0 })
			{
				rect.pos += settings.offset;
				settings.offset = { 0, 0 };
			}
			for (auto p : rect.Range<TOP_TO_BOTTOM, LEFT_TO_RIGHT>())
			{
				if (!bitsLeft)
				{
					bits = *data;
					data += 1;
					bitsLeft = 8;
				}
				auto alpha = (bits & 3) * 0x55;
				bits >>= 2;
				bitsLeft -= 2;
				blend(p, effectiveColor.WithAlpha(alpha));
			}
			if (settings.underline)
			{
				auto underlineRect = rect;
				underlineRect.pos.Y += fontTypeSize + 2;
				underlineRect.size.Y = 1;
				for (auto p : underlineRect)
				{
					blend(p, effectiveColor.WithAlpha(0xFF));
				}
			}
#if DebugGuiHost
			for (int32_t x = 0; x < rect.size.X; ++x)
			{
				blend(rect.pos + Point{ x,               0 }, effectiveColor.WithAlpha(0x80));
				blend(rect.pos + Point{ x, rect.size.Y - 1 }, effectiveColor.WithAlpha(0x80));
			}
			for (int32_t y = 1; y < rect.size.Y - 1; ++y)
			{
				blend(rect.pos + Point{               0, y }, effectiveColor.WithAlpha(0x80));
				blend(rect.pos + Point{ rect.size.X - 1, y }, effectiveColor.WithAlpha(0x80));
			}
#endif
			++regionIt;
		};
		auto regionIt = regions.begin();
		auto regionEnd = regions.end();
		size_t it = 0;
		Utf8Character ch;
		auto advance = [&it, &ch, &text]() {
			it += ch.length;
			ch = Utf8Next(text, it);
			return ch;
		};
		for (; it != text.size() && regionIt != regionEnd; it += ch.length)
		{
			ch = Utf8Next(text, it);
			if (!ch.valid)
			{
				continue;
			}
			switch (ch.codePoint)
			{
			case '\x12':
				settings.add = !settings.add;
				break;

			case '\x11':
				advance();
				break;

			case '\x10':
				{
					// TODO-REDO_UI: kinda dumb; not using all 8 bits because then \u0000 would be common
					// and string functions don't really like \u0000, so \u0080 can be used instead
					auto offx = int32_t(advance().codePoint) & 0x7F;
					auto offy = int32_t(advance().codePoint) & 0x7F;
					if (offx >= 0x40) offx -= 0x80;
					if (offy >= 0x40) offy -= 0x80;
					settings.offset = { offx, offy };
				}
				break;

			case '\x0F':
				{
					auto rch = advance().codePoint;
					auto gch = advance().codePoint;
					auto bch = advance().codePoint;
					settings.color = Rgb8(
						uint8_t(rch),
						uint8_t(gch),
						uint8_t(bch)
					);
				}
				break;

			case '\x0E':
				settings = {};
				break;

			case '\x01':
				settings.invert = !settings.invert;
				break;

			case '\b':
				{
					auto ech = advance().codePoint;
					switch (ech)
					{
					case 'w': settings.color = colorWhite      ; break;
					case 'g': settings.color = colorGray       ; break;
					case 'o': settings.color = colorYellow     ; break;
					case 'r': settings.color = colorRed        ; break;
					case 'l': settings.color = colorLightRed   ; break;
					case 'b': settings.color = colorBlue       ; break;
					case 't': settings.color = colorTeal       ; break;
					case 'u': settings.color = colorPurple     ; break;
					case 'i': settings.color = colorTitlePurple; break;

					case 'U':
						settings.underline = !settings.underline;
						break;
					}
				}
				break;

			default:
				takeRegion(shapedText.stp.replaceWithDots ? Gui::iconBullet : ch.codePoint, regionIt);
				break;
			}
		}
		{
			auto &overflowRegions = shapedText.wrapper.GetOverflowRegions();
			auto overflowRegionIt = overflowRegions.begin();
			auto overflowRegionEnd = overflowRegions.end();
			while (overflowRegionIt != overflowRegionEnd)
			{
				takeRegion(overflowRegionIt->codePoint, overflowRegionIt);
			}
		}
		shapedText.texture = std::make_unique<StaticTexture>(*this, true, std::move(textureData));
	}

	Host::Host()
	{
		SdlAssertZero(SDL_Init(SDL_INIT_VIDEO));
		SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);
		SDL_EventState(SDL_DROPBEGIN, SDL_DISABLE);
		SDL_EventState(SDL_DROPCOMPLETE, SDL_DISABLE);
		// SdlAssertTrue(SDL_SetHintWithPriority(SDL_HINT_IME_SHOW_UI, "1", SDL_HINT_OVERRIDE)); // TODO-REDO_UI
		LoadFontData();
	}

	Host::~Host()
	{
		SetStack(nullptr);
		Close();
		SDL_Quit();
	}

	void Host::LoadFontData()
	{
		fontData = Bz2Decompress(FontBz2.AsCharSpan(), std::nullopt);
		auto *begin = reinterpret_cast<const uint8_t *>(fontData.data());
		auto *ptr = begin;
		auto *end = ptr + fontData.size();
		std::optional<uint32_t> firstCodePoint;
		std::optional<uint32_t> lastCodePoint;
		int32_t lastPtrBase = 0;
		auto closeRange = [this, &firstCodePoint, &lastCodePoint, &lastPtrBase]() {
			if (firstCodePoint && lastCodePoint)
			{
				fontRanges.push_back({ *firstCodePoint, *lastCodePoint, lastPtrBase });
				firstCodePoint.reset();
			}
		};
		while (ptr != end)
		{
			Assert(ptr + 4 <= end);
			auto codePoint = (uint32_t(ptr[0])      ) |
			                 (uint32_t(ptr[1]) <<  8) |
			                 (uint32_t(ptr[2]) << 16);
			Assert(codePoint < 0x110000);
			auto width = ptr[3];
			Assert(width <= 64);
			auto ptrBase = int32_t(fontPtrs.size());
			fontPtrs.push_back(ptr + 3 - begin);
			ptr += 4 + width * 3;
			Assert(ptr <= end);
			Assert(lastCodePoint < codePoint);
			if (lastCodePoint && *lastCodePoint + 1U < codePoint)
			{
				closeRange();
			}
			if (!firstCodePoint)
			{
				firstCodePoint = codePoint;
				lastPtrBase = ptrBase;
			}
			lastCodePoint = codePoint;
		}
		closeRange();
	}

	void Host::CreateWindow()
	{
		if (sdlWindow)
		{
			return;
		}
		sdlWindow = SdlAssertPtr(SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 100, 100, SDL_WINDOW_HIDDEN));
		if constexpr (SET_WINDOW_ICON)
		{
			WindowIcon(sdlWindow);
		}
	}

	void Host::DestroyWindow()
	{
		if (!sdlWindow)
		{
			return;
		}
		DestroyRenderer();
		SDL_DestroyWindow(sdlWindow);
		sdlWindow = nullptr;
	}

	void Host::CreateRenderer()
	{
		if (sdlRenderer)
		{
			return;
		}
		CreateWindow();
		SdlAssertTrue(SDL_SetHintWithPriority(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1", SDL_HINT_OVERRIDE));
		Uint32 rendererFlags = SDL_RENDERER_TARGETTEXTURE | SDL_RENDERER_PRESENTVSYNC;
		sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, rendererFlags);
		if (!sdlRenderer && (rendererFlags & SDL_RENDERER_PRESENTVSYNC))
		{
			rendererFlags ^= SDL_RENDERER_PRESENTVSYNC;
			sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, rendererFlags);
			explicitDelay = true; // TODO-REDO_UI: not true for emscripten
		}
		SdlAssertPtr(sdlRenderer);
		SdlAssertZero(SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND));
		SdlAssertZero(SDL_RenderSetLogicalSize(sdlRenderer, windowParameters.windowSize.X, windowParameters.windowSize.Y));
		sdlTexture = SdlAssertPtr(SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, windowParameters.windowSize.X, windowParameters.windowSize.Y));
	}

	void Host::DestroyRenderer()
	{
		if (!sdlRenderer)
		{
			return;
		}
		if (stackHolder)
		{
			stackHolder->SetRendererUp(false);
		}
		for (auto *staticTexture : staticTextures)
		{
			DestroyStaticTextureTexture(*staticTexture);
		}
		SDL_DestroyTexture(sdlTexture);
		sdlTexture = nullptr;
		SDL_DestroyRenderer(sdlRenderer);
		sdlRenderer = nullptr;
	}

	void Host::Close()
	{
		DestroyWindow();
	}

	void Host::Open()
	{
		CreateRenderer();
		SdlAssertZero(SDL_SetTextureScaleMode(sdlTexture, windowParameters.blurryScaling ? SDL_ScaleModeLinear : SDL_ScaleModeNearest));
		bool forceIntegerScale = false;
		Uint32 fullscreenFlags = 0;
		if (windowParameters.frameType == WindowParameters::FrameType::fullscreen)
		{
			forceIntegerScale = windowParameters.fullscreenForceIntegerScale;
			fullscreenFlags = SDL_WINDOW_FULLSCREEN_DESKTOP;
			if (windowParameters.fullscreenChangeResolution)
			{
				fullscreenFlags = SDL_WINDOW_FULLSCREEN;
			}
		}
		SdlAssertZero(SDL_SetWindowFullscreen(sdlWindow, fullscreenFlags)); // do this first; seems to override other settings if done later...
		SdlAssertZero(SDL_RenderSetIntegerScale(sdlRenderer, forceIntegerScale ? SDL_TRUE : SDL_FALSE));
		SDL_SetWindowResizable(sdlWindow, windowParameters.frameType == WindowParameters::FrameType::resizable ? SDL_TRUE : SDL_FALSE);
		auto scaledWindowSize = windowParameters.GetScaledWindowSize();
		SDL_SetWindowSize(sdlWindow, scaledWindowSize.X, scaledWindowSize.Y);
		SDL_ShowWindow(sdlWindow);
	}

	void Host::Start()
	{
		Open();
		running = true;
	}

	void Host::Stop()
	{
		running = false;
	}

	bool Host::IsRunning() const
	{
		return running;
	}

	void Host::HandleFrame()
	{
		int32_t repeatCount = 0;
		while (repeatCount < maxFrameRepeatCount)
		{
			repeatCount += 1;
			auto currentWindowParameters = windowParameters;
			{
				auto currentStackHolder = stackHolder;
				internedTextStore.BeginFrame();
				shapedTextStore.BeginFrame();
				lastTick = Ticks(SDL_GetTicks64());
				if (currentStackHolder)
				{
					currentStackHolder->SetRendererUp(true);
					currentStackHolder->SetOnTop(true);
					currentStackHolder->stack->HandleFrame(sdlTexture);
				}
				shapedTextStore.EndFrame();
				internedTextStore.EndFrame();
				// currentStackHolder goes out of scope here and potentially calls ~StackHolder
			}
			if (auto recreate = currentWindowParameters.Compare(windowParameters))
			{
				if (recreate->renderer)
				{
					DestroyRenderer();
				}
				if (recreate->window)
				{
					DestroyWindow();
				}
				Open();
			}
			else
			{
				break;
			}
		}
#if DebugGuiHost
		if (repeatCount > 1)
		{
			Log("Host::HandleFrame repeatCount ", repeatCount);
		}
#endif
		SdlAssertZero(SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 0xFF));
		SdlAssertZero(SDL_RenderClear(sdlRenderer));
		SdlAssertZero(SDL_RenderCopy(sdlRenderer, sdlTexture, nullptr, nullptr));
		SDL_RenderPresent(sdlRenderer);
#if DebugGuiHost && DebugCrossFrameItemStore
		Log("internedTextStore stats: ", internedTextStore.debugAllocCount, " ", internedTextStore.debugFreeCount, " ", internedTextStore.debugAliveCount, " ", internedTextStore.debugItemsSize, " ", internedTextStore.textData.size());
#endif
		if (!stackHolder || stackHolder->stack->IsEmpty())
		{
			Stop();
			return;
		}
		if (explicitDelay)
		{
			SDL_Delay(explicitDelayMs);
		}
	}

	void Host::SetStack(std::shared_ptr<Stack> newStack)
	{
		stackHolder.reset();
		if (newStack)
		{
			stackHolder = std::make_shared<StackHolder>(*this, newStack);
		}
	}

	void Host::SetClipRect(Rect newClipRect)
	{
		clipRect = newClipRect;
		SDL_Rect sdlRect;
		sdlRect.x = clipRect.pos.X;
		sdlRect.y = clipRect.pos.Y;
		sdlRect.w = clipRect.size.X;
		sdlRect.h = clipRect.size.Y;
		SdlAssertZero(SDL_RenderSetClipRect(sdlRenderer, &sdlRect));
	}

	Rect Host::GetClipRect() const
	{
		return clipRect;
	}

	ShapedTextIndex Host::ShapeText(InternedTextIndex text, std::optional<int32_t> maxWidth)
	{
		std::optional<ShapeTextParameters::MaxWidth> stpMaxWidth;
		if (maxWidth)
		{
			stpMaxWidth = ShapeTextParameters::MaxWidth{ *maxWidth, false };
		}
		return ShapeText(ShapeTextParameters{ text, stpMaxWidth });
	}

	ShapedTextIndex Host::ShapeText(ShapeTextParameters stp)
	{
		// TODO-REDO_UI: heuristic
		if (auto index = shapedTextStore.GetIndex(stp))
		{
			shapedTextStore[*index].state = ShapedText::State::used;
			return *index;
		}
		auto index = shapedTextStore.Alloc(stp);
		shapedTextStore[index].state = ShapedText::State::used;
		return index;
	}

	InternedTextIndex Host::InternText(std::string_view text)
	{
		// TODO-REDO_UI: heuristic
		if (auto index = internedTextStore.GetIndex(text))
		{
			internedTextStore[*index].state = InternedText::State::used;
			return *index;
		}
		auto index = internedTextStore.Alloc(text);
		internedTextStore[index].state = InternedText::State::used;
		return index;
	}

	void Host::DrawRect(Rect rect, Rgba8 color)
	{
		SdlAssertZero(SDL_SetRenderDrawColor(sdlRenderer, color.Red, color.Green, color.Blue, color.Alpha));
		SDL_Rect sdlRect;
		sdlRect.x = rect.pos.X;
		sdlRect.y = rect.pos.Y;
		sdlRect.w = rect.size.X;
		sdlRect.h = rect.size.Y;
		SdlAssertZero(SDL_RenderDrawRect(sdlRenderer, &sdlRect));
	}

	void Host::FillRect(Rect rect, Rgba8 color)
	{
		SdlAssertZero(SDL_SetRenderDrawColor(sdlRenderer, color.Red, color.Green, color.Blue, color.Alpha));
		SDL_Rect sdlRect;
		sdlRect.x = rect.pos.X;
		sdlRect.y = rect.pos.Y;
		sdlRect.w = rect.size.X;
		sdlRect.h = rect.size.Y;
		SdlAssertZero(SDL_RenderFillRect(sdlRenderer, &sdlRect));
	}

	void Host::DrawPoint(Point pos, Rgba8 color)
	{
		SdlAssertZero(SDL_SetRenderDrawColor(sdlRenderer, color.Red, color.Green, color.Blue, color.Alpha));
		SdlAssertZero(SDL_RenderDrawPoint(sdlRenderer, pos.X, pos.Y));
	}

	void Host::DrawLine(Point from, Point to, Rgba8 color)
	{
		SdlAssertZero(SDL_SetRenderDrawColor(sdlRenderer, color.Red, color.Green, color.Blue, color.Alpha));
		SdlAssertZero(SDL_RenderDrawLine(sdlRenderer, from.X, from.Y, to.X, to.Y));
	}

	void Host::DrawText(Point pos, ShapedTextIndex text, Rgba8 color)
	{
		auto &shapedText = shapedTextStore[text];
		if (!shapedText.texture)
		{
			CreateShapedTextTexture(shapedText);
		}
		DrawStaticTexture({ pos.X, pos.Y - fontCapLine }, *shapedText.texture, color);
#if DebugGuiHost
		DrawRect({ pos, shapedText.wrapper.GetContentSize() }, 0x8000FF00_argb);
#endif
	}

	void Host::DrawText(Point pos, InternedTextIndex text, Rgba8 color)
	{
		DrawText(pos, ShapeText(text), color);
	}

	void Host::DrawText(Point pos, std::string_view text, Rgba8 color)
	{
		DrawText(pos, InternText(text), color);
	}

	void Host::DrawStaticTexture(Rect from, Rect to, StaticTexture &staticTexture, Rgba8 color)
	{
		if (!staticTexture.haveTexture)
		{
			CreateStaticTextureTexture(staticTexture);
		}
		if (staticTexture.texture)
		{
			SdlAssertZero(SDL_SetTextureColorMod(staticTexture.texture, color.Red, color.Green, color.Blue));
			SdlAssertZero(SDL_SetTextureAlphaMod(staticTexture.texture, color.Alpha));
			SDL_Rect sdlRectFrom;
			sdlRectFrom.x = from.pos.X;
			sdlRectFrom.y = from.pos.Y;
			sdlRectFrom.w = from.size.X;
			sdlRectFrom.h = from.size.Y;
			SDL_Rect sdlRectTo;
			sdlRectTo.x = to.pos.X;
			sdlRectTo.y = to.pos.Y;
			sdlRectTo.w = to.size.X;
			sdlRectTo.h = to.size.Y;
			SdlAssertZero(SDL_RenderCopy(sdlRenderer, staticTexture.texture, &sdlRectFrom, &sdlRectTo));
#if DebugGuiHost
			DrawRect(to, 0xFF008000_argb);
#endif
		}
	}

	void Host::DrawStaticTexture(Rect to, StaticTexture &staticTexture, Rgba8 color)
	{
		DrawStaticTexture(staticTexture.data.Size().OriginRect(), to, staticTexture, color);
	}

	void Host::DrawStaticTexture(Point to, StaticTexture &staticTexture, Rgba8 color)
	{
		DrawStaticTexture(Rect{ to, staticTexture.data.Size() }, staticTexture, color);
	}

	const uint8_t *Host::CharData(uint32_t ch) const
	{
		auto it = std::upper_bound(fontRanges.begin(), fontRanges.end(), ch, [](uint32_t ch, auto &range) {
			return ch < range.first;
		});
		if (it != fontRanges.begin())
		{
			auto &range = it[-1];
			if (ch <= range.last)
			{
				return reinterpret_cast<const uint8_t *>(fontData.data()) + fontPtrs[range.ptrBase + (ch - range.first)];
			}
		}
		Assert(ch != 0xFFFD);
		return CharData(0xFFFD);
	}

	std::string_view Host::InternedTextStore::DereferenceSpan(InternedTextStoreSpan span) const
	{
		return std::string_view(textData.data() + span.base, span.size);
	}

	bool Host::InternedTextStore::EndFrame()
	{
		auto removedSome = CrossFrameItemStore::EndFrame();
		if (removedSome)
		{
			PruneText();
		}
		return removedSome;
	}

	std::string_view Host::GetInternedText(InternedTextIndex index) const
	{
		return internedTextStore.DereferenceSpan(internedTextStore[index].text);
	}

	const TextWrapper &Host::GetShapedTextWrapper(ShapedTextIndex index) const
	{
		return shapedTextStore[index].wrapper;
	}

	int32_t Host::CharWidth(uint32_t ch) const
	{
		return *CharData(ch);
	}

	Host::StackHolder::StackHolder(Host &newHost, std::shared_ptr<Stack> newStack) : host(newHost), stack(newStack)
	{
	}

	Host::StackHolder::~StackHolder()
	{
		SetOnTop(false);
		SetRendererUp(false);
	}

	void Host::StackHolder::SetRendererUp(bool newRendererUp)
	{
		if (stack->GetRendererUp() != newRendererUp)
		{
			stack->SetRendererUp(newRendererUp);
		}
	}

	void Host::StackHolder::SetOnTop(bool newOnTop)
	{
		if (stack->GetOnTop() != newOnTop)
		{
			stack->SetOnTop(newOnTop);
		}
	}

	void Host::StartTextInput()
	{
		SDL_StartTextInput();
		SDL_Rect sdlRect; // TODO-REDO_UI
		sdlRect.x = 10;
		sdlRect.y = 10;
		sdlRect.w = 30;
		sdlRect.h = 30;
		SDL_SetTextInputRect(&sdlRect);
	}

	void Host::StopTextInput()
	{
		SDL_StopTextInput();
	}

	void Host::CopyTextToClipboard(std::string text)
	{
		SDL_SetClipboardText(text.c_str());
	}

	std::optional<std::string> Host::PasteTextFromClipboard()
	{
		if (auto *text = SDL_GetClipboardText())
		{
			return text;
		}
		return std::nullopt;
	}

	Point Host::GetDesktopSize() const
	{
		int32_t displayIndex = SDL_GetWindowDisplayIndex(sdlWindow);
		if (displayIndex >= 0)
		{
			SDL_Rect rect;
			if (!SDL_GetDisplayUsableBounds(displayIndex, &rect))
			{
				return { rect.w, rect.h };
			}
		}
		return { 1280, 1024 }; // Return something vaguely sensible.
	}

	void Host::AddStaticTexture(StaticTexture &staticTexture)
	{
		staticTextures.push_front(&staticTexture);
		staticTexture.hostIt = staticTextures.begin();
	}

	void Host::RemoveStaticTexture(StaticTexture &staticTexture)
	{
		DestroyStaticTextureTexture(staticTexture);
		staticTextures.erase(staticTexture.hostIt);
	}

	void Host::CreateStaticTextureTexture(StaticTexture &staticTexture)
	{
		auto size = staticTexture.data.Size();
		if (size.X && size.Y) // SDL doesn't like empty textures.
		{
			staticTexture.texture = SdlAssertPtr(SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, size.X, size.Y));
			SDL_UpdateTexture(staticTexture.texture, nullptr, staticTexture.data.data(), size.X * sizeof(uint32_t));
			SdlAssertZero(SDL_SetTextureScaleMode(staticTexture.texture, SDL_ScaleModeLinear));
			if (staticTexture.blend)
			{
				SdlAssertZero(SDL_SetTextureBlendMode(staticTexture.texture, SDL_BLENDMODE_BLEND));
			}
		}
		// We set this even if we didn't actually create the texture for consistency.
		staticTexture.haveTexture = true;
	}

	void Host::DestroyStaticTextureTexture(StaticTexture &staticTexture)
	{
		SDL_DestroyTexture(staticTexture.texture);
		staticTexture.texture = nullptr;
		staticTexture.haveTexture = false;
	}
}
