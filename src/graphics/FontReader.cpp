#include "FontReader.h"

#include "bzip2/bz2wrap.h"
#include "font_bz2.h"
#include "common/Defer.h"

#include <array>
#include <cstdint>
#include <SDL2/SDL_ttf.h>
#include <iostream>

unsigned char *font_data = nullptr;
unsigned int *font_ptrs = nullptr;
unsigned int (*font_ranges)[2] = nullptr;

FontReader::FontReader(unsigned char const *_pointer):
	pointer(_pointer + 1),
	width(*_pointer),
	pixels(0),
	data(0)
{
}

static bool InitFontData()
{
	static std::vector<char> fontDataBuf;
	static std::vector<int> fontPtrsBuf;
	static std::vector< std::array<int, 2> > fontRangesBuf;
	if (BZ2WDecompress(fontDataBuf, font_bz2.AsCharSpan()) != BZ2WDecompressOk)
	{
		return false;
	}
	int first = -1;
	int last = -1;
	char *begin = fontDataBuf.data();
	char *ptr = fontDataBuf.data();
	char *end = fontDataBuf.data() + fontDataBuf.size();
	while (ptr != end)
	{
		if (ptr + 4 > end)
		{
			return false;
		}
		auto codePoint = *reinterpret_cast<uint32_t *>(ptr) & 0xFFFFFFU;
		if (codePoint >= 0x110000U)
		{
			return false;
		}
		auto width = *reinterpret_cast<uint8_t *>(ptr + 3);
		if (width > 64)
		{
			return false;
		}
		if (ptr + 4 + width * 3 > end)
		{
			return false;
		}
		auto cp = (int)codePoint;
		if (last >= cp)
		{
			return false;
		}
		if (first != -1 && last + 1 < cp)
		{
			fontRangesBuf.push_back({ { first, last } });
			first = -1;
		}
		if (first == -1)
		{
			first = cp;
		}
		last = cp;
		fontPtrsBuf.push_back(ptr + 3 - begin);
		ptr += width * 3 + 4;
	}
	if (first != -1)
	{
		fontRangesBuf.push_back({ { first, last } });
	}
	fontRangesBuf.push_back({ { 0, 0 } });
	font_data = reinterpret_cast<unsigned char *>(fontDataBuf.data());
	font_ptrs = reinterpret_cast<unsigned int *>(fontPtrsBuf.data());
	font_ranges = reinterpret_cast<unsigned int (*)[2]>(fontRangesBuf.data());
	return true;
}

bool ttfInitDone = false;
bool ttfOk = false;
TTF_Font *ttf = NULL;
int ttfOffsetX = 0;
int ttfOffsetY = 4;
int ttfSize = 12;

static void ttfInit()
{
	ttfInitDone = true;
	if (TTF_Init() < 0)
	{
		std::cerr << "TTF_Init failed: " << TTF_GetError() << std::endl;
		return;
	}
	// ttf = TTF_OpenFont("zpix.ttf", 12);
	ttf = TTF_OpenFont("/usr/share/fonts/noto-cjk/NotoSerifCJK-Regular.ttc", ttfSize);
	if (!ttf)
	{
		std::cerr << "TTF_OpenFont failed: " << TTF_GetError() << std::endl;
		return;
	}
	ttfOk = true;
	TTF_SetFontStyle(ttf, TTF_STYLE_NORMAL);
	TTF_SetFontOutline(ttf, 0);
	TTF_SetFontKerning(ttf, 1);
	TTF_SetFontHinting(ttf, TTF_HINTING_NONE);
}

std::vector<unsigned char> ttfBytes;
static int ttfRenderChar(String::value_type ch)
{
	if (!ttfInitDone)
	{
		ttfInit();
	}
	if (!ttfOk)
	{
		return -1;
	}
	if (!TTF_GlyphIsProvided32(ttf, ch))
	{
		return -1;
	}
	SDL_Color fg{ 255, 255, 255, 255 };
	auto *surf = TTF_RenderGlyph32_Blended(ttf, ch, fg);
	if (!surf)
	{
		std::cerr << "TTF_RenderGlyph32_Blended failed: " << TTF_GetError() << std::endl;
		return -1;
	}
	Defer freeSurface([surf]() {
		SDL_FreeSurface(surf);
	});
	if (SDL_LockSurface(surf))
	{
		std::cerr << "SDL_LockSurface failed: " << SDL_GetError() << std::endl;
		return -1;
	}
	Defer unlockSurface([surf]() {
		SDL_UnlockSurface(surf);
	});
	auto base = int(ttfBytes.size());
	auto access = [surf](int x, int y) -> uint8_t {
		auto *pixels = reinterpret_cast<uint8_t *>(surf->pixels);
		if (x >= 0 && y >= 0 && x < surf->w && y < surf->h)
		{
			return pixels[y * surf->pitch + x * 4 + 3]; // get alpha channel
		}
		return 0;
	};
	uint8_t width = surf->w;
	static_assert(FONT_H % 4 == 0);
	ttfBytes.resize(base + 1 + width * (FONT_H / 4), 0);
	ttfBytes[base] = width;
	for (auto x = 0; x < width; ++x)
	{
		for (auto y = 0; y < FONT_H; ++y)
		{
			auto pixel = access(x + ttfOffsetX, y + ttfOffsetY);
			uint8_t pixel4 = 0;
			     if (pixel > 140) pixel4 = 3;
			else if (pixel >  80) pixel4 = 2;
			else if (pixel >  40) pixel4 = 1;
			auto index = y * width + x;
			ttfBytes[base + 1 + (index / 4)] |= pixel4 << (index % 4 * 2);
		}
	}
	return base;
}

std::unordered_map<String::value_type, int> ttfOffsets;
static const unsigned char *ttfGetChar(String::value_type ch)
{
	auto it = ttfOffsets.find(ch);
	if (it == ttfOffsets.end())
	{
		ttfOffsets.insert({ ch, ttfRenderChar(ch) });
		it = ttfOffsets.find(ch);
	}
	if (it->second == -1)
	{
		return nullptr;
	}
	return ttfBytes.data() + it->second;
}

unsigned char const *FontReader::lookupChar(String::value_type ch)
{
	if (!font_data)
	{
		if (!InitFontData())
		{
			throw std::runtime_error("font data corrupt");
		}
	}
	if (ch >= 0xE000 && ch < 0xF8FF)
	{
		size_t offset = 0;
		for(int i = 0; font_ranges[i][1]; i++)
			if(font_ranges[i][0] > ch)
				break;
			else if(font_ranges[i][1] >= ch)
				return &font_data[font_ptrs[offset + (ch - font_ranges[i][0])]];
			else
				offset += font_ranges[i][1] - font_ranges[i][0] + 1;
	}
	auto *ttfChar = ttfGetChar(ch);
	if (ttfChar)
	{
		return ttfChar;
	}
	if(ch == 0xFFFD)
		return &font_data[0];
	else
		return lookupChar(0xFFFD);
}

FontReader::FontReader(String::value_type ch):
	FontReader(lookupChar(ch))
{
}

int FontReader::GetWidth() const
{
	return width;
}

int FontReader::NextPixel()
{
	if(!pixels)
	{
		data = *(pointer++);
		pixels = 4;
	}
	int old = data;
	pixels--;
	data >>= 2;
	return old & 0x3;
}
