#include "Utf8.hpp"
#include <algorithm>
#include <array>

namespace Powder
{
	constexpr bool AreStringLiteralsUtf8()
	{
		const char srcCharSet[] = "zß水🍌";
		const char dstCharSet[] = "\u007A\u00DF\u6C34\U0001F34C";
		std::array<uint8_t, 11> expect = { 0x7A, 0xC3, 0x9F, 0xE6, 0xB0, 0xB4, 0xF0, 0x9F, 0x8D, 0x8C, 0x00 };
		return std::ranges::equal(expect, srcCharSet, [](auto lhs, auto rhs) {
			return lhs == uint8_t(rhs);
		}) && std::ranges::equal(dstCharSet, srcCharSet);
	}
	static_assert(AreStringLiteralsUtf8());

	Utf8Iterator::Character Utf8Iterator::Decode() const
	{
		auto *u8 = reinterpret_cast<const uint8_t *>(view.data() + pos);
		UnicodeCodePoint head = u8[0]; // UB if (view.size() - pos) == 0, which is fine
		auto checkHead = [&head](uint8_t minValue, uint8_t maxValue, uint8_t mask) {
			if (head < minValue || head > maxValue)
			{
				return false;
			}
			head &= mask;
			return true;
		};
		size_t decodedLength = 1;
		auto consume = [this, &decodedLength, &head, u8](uint8_t minValue, uint8_t maxValue) {
			if ((view.size() - pos) <= decodedLength)
			{
				return false;
			}
			auto value = u8[decodedLength];
			if (value < minValue || value > maxValue)
			{
				return false;
			}
			head = (head << 6) | (value & UINT8_C(0x3F));
			decodedLength += 1;
			return true;
		};
		bool ok = (checkHead(0x00, 0x7F, 0xFF)) ||
		          (checkHead(0xC2, 0xDF, 0x1F) && consume(0x80, 0xBF)) ||
		          (checkHead(0xE0, 0xE0, 0x0F) && consume(0xA0, 0xBF) && consume(0x80, 0xBF)) ||
		          (checkHead(0xE1, 0xEC, 0x0F) && consume(0x80, 0xBF) && consume(0x80, 0xBF)) ||
		          (checkHead(0xED, 0xED, 0x0F) && consume(0x80, 0x9F) && consume(0x80, 0xBF)) ||
		          (checkHead(0xEE, 0xEF, 0x0F) && consume(0x80, 0xBF) && consume(0x80, 0xBF)) ||
		          (checkHead(0xF0, 0xF0, 0x07) && consume(0x90, 0xBF) && consume(0x80, 0xBF) && consume(0x80, 0xBF)) ||
		          (checkHead(0xF1, 0xF3, 0x07) && consume(0x80, 0xBF) && consume(0x80, 0xBF) && consume(0x80, 0xBF)) ||
		          (checkHead(0xF4, 0xF4, 0x07) && consume(0x80, 0x8F) && consume(0x80, 0xBF) && consume(0x80, 0xBF));
		return { ok, decodedLength, ok ? head : 0xFFFD };
	}

	Utf8Iterator::Character Utf8Iterator::operator *() const
	{
		return Decode();
	}

	Utf8Iterator &Utf8Iterator::operator ++()
	{
		auto decoded = Decode();
		pos += decoded.length;
		chars += 1;
		return *this;
	}

	Utf8Iterator Utf8Begin(std::string_view view)
	{
		return { view, 0 };
	}

	Utf8Iterator Utf8End(std::string_view view)
	{
		return { view, view.size() };
	}

	Utf8Range::Utf8Range(std::string_view newView) : view(newView)
	{
	}

	Utf8Iterator Utf8Range::begin()
	{
		return Utf8Begin(view);
	}

	Utf8Iterator Utf8Range::end()
	{
		return Utf8End(view);
	}

	size_t Utf8Append(std::string &str, UnicodeCodePoint codePoint)
	{
		auto oldSize = str.size();
		if (codePoint >= 0x110000 || (codePoint >= 0xD800 && codePoint < 0xE000))
		{
			codePoint = 0xFFFD;
		}
		if (codePoint >= 0x10000)
		{
			str.append(1, 0xF0 | ((codePoint >> 18)       ));
			str.append(1, 0x80 | ((codePoint >> 12) & 0x3F));
			str.append(1, 0x80 | ((codePoint >>  6) & 0x3F));
			str.append(1, 0x80 | ( codePoint        & 0x3F));
		}
		else if (codePoint >= 0x800)
		{
			str.append(1, 0xE0 | ((codePoint >> 12)       ));
			str.append(1, 0x80 | ((codePoint >>  6) & 0x3F));
			str.append(1, 0x80 | ( codePoint        & 0x3F));
		}
		else if (codePoint >= 0x80)
		{
			str.append(1, 0xC0 | ((codePoint >>  6)       ));
			str.append(1, 0x80 | ( codePoint        & 0x3F));
		}
		else
		{
			str.append(1, codePoint);
		}
		return str.size() - oldSize;
	}
}
