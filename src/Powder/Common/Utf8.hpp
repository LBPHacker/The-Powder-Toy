#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace Powder
{
	using UnicodeCodePoint = uint32_t;

	struct Utf8Iterator
	{
		std::string_view view;
		size_t pos = 0;
		size_t chars = 0; // TODO-REDO_UI: maybe remove

		struct Character
		{
			bool valid = false;
			size_t length = 0;
			UnicodeCodePoint codePoint = 0;
		};
		Character Decode() const;

		bool operator ==(const Utf8Iterator &other) const
		{
			return pos == other.pos;
		}

		bool operator !=(const Utf8Iterator &other) const
		{
			return !(*this == other);
		}

		Character operator *() const;
		Utf8Iterator &operator ++();
	};

	Utf8Iterator Utf8Begin(std::string_view view);
	Utf8Iterator Utf8End(std::string_view view);

	struct Utf8Range
	{
		std::string_view view;

		Utf8Range(std::string_view newView = {});

		Utf8Iterator begin();
		Utf8Iterator end();
	};

	size_t Utf8Append(std::string &str, UnicodeCodePoint codePoint);
}
