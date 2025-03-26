#pragma once
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>

namespace Powder
{
	using UnicodeCodePoint = uint32_t;
	struct Utf8Character
	{
		bool valid = false;
		size_t length = 0;
		UnicodeCodePoint codePoint = 0;
	};
	Utf8Character Utf8Next(std::string_view view, size_t pos); // UB if pos >= view.size()
	Utf8Character Utf8Prev(std::string_view view, size_t pos); // UB if pos == 0
	size_t Utf8Append(std::string &str, UnicodeCodePoint codePoint);
}
