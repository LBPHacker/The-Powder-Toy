#include "FontData.h"

#include "bzip2/bz2wrap.h"
#include "font.bz2.h"

#include <array>
#include <vector>
#include <stdexcept>

namespace gui
{
	struct InitFontData : public FontData
	{
		InitFontData()
		{
			static std::vector<char> fontDataBuf;
			static std::vector<int> fontPtrsBuf;
			static std::vector< std::array<int, 2> > fontRangesBuf;
			if (BZ2WDecompress(fontDataBuf, reinterpret_cast<const char *>(compressed_font_data), compressed_font_data_size) != BZ2WDecompressOk)
			{
				throw std::runtime_error("failed to decompress font data");
			}
			int first = -1;
			int last = -1;
			char *begin = &fontDataBuf[0];
			char *ptr = &fontDataBuf[0];
			char *end = &fontDataBuf[0] + fontDataBuf.size();
			while (ptr != end)
			{
				if (ptr + 4 > end)
				{
					throw std::runtime_error("premature end of font data");
				}
				auto codePoint = *reinterpret_cast<uint32_t *>(ptr) & 0xFFFFFFU;
				if (codePoint >= 0x110000U)
				{
					throw std::runtime_error("invalid code point in font data");
				}
				auto width = *reinterpret_cast<uint8_t *>(ptr + 3);
				if (width > 64)
				{
					throw std::runtime_error("invalid width in font data");
				}
				if (ptr + 4 + width * 3 > end)
				{
					throw std::runtime_error("premature end of font data");
				}
				auto cp = (int)codePoint;
				if (last >= cp)
				{
					throw std::runtime_error("order violation in font data");
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
			bits = reinterpret_cast<unsigned char *>(fontDataBuf.data());
			ptrs = reinterpret_cast<unsigned int *>(fontPtrsBuf.data());
			ranges = reinterpret_cast<unsigned int (*)[2]>(fontRangesBuf.data());
		}
	};

	const FontData &GetFontData()
	{
		static InitFontData ifd;
		return static_cast<FontData &>(ifd);
	}
}
