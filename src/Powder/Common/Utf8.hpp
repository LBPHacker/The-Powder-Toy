#pragma once
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>

namespace Powder
{
	using UnicodeCodePoint = uint32_t;

	struct Utf8Iterator : public std::counted_iterator<std::string_view::const_iterator>
	{
		using counted_iterator::counted_iterator;

		struct Character
		{
			bool valid = false;
			size_t length = 0;
			UnicodeCodePoint codePoint = 0;
		};
		Character Decode() const;

		Character operator *() const
		{
			return Decode();
		}

		Utf8Iterator &operator ++();
		Utf8Iterator operator ++(int) const;
	};

	struct Utf8Range
	{
		std::string_view view;

		auto begin()
		{
			return Utf8Iterator(view.begin(), view.size());
		}

		auto end()
		{
			return std::default_sentinel;
		}
	};

	size_t Utf8Append(std::string &str, UnicodeCodePoint codePoint);
}
