#pragma once
#include "common/String.h"
#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <stddef.h>
#include <string>
#include <variant>

namespace Powder::Lang
{
	struct FormatterHolder;

	FormatterHolder &GetFormatterHolder(const char *name);

	template<std::size_t CharCount>
	struct TranslationNameHolder
	{
		char chars[CharCount]{};
		
		constexpr TranslationNameHolder(const char (&newChars)[CharCount])
		{
			std::ranges::copy(newChars, chars);
		}
	};

	using FormatParam = std::variant<
		std::string_view,
		int64_t,
		float
	>;

	template<class Thing> struct FormatParamHelper;
	template<> struct FormatParamHelper<std::string>  { static FormatParam Make(const std::string &str) { return std::string_view(str); } };
	template<> struct FormatParamHelper<ByteString>   { static FormatParam Make(const ByteString &str ) { return std::string_view(str); } };
	template<> struct FormatParamHelper<const char *> { static FormatParam Make(const char *str       ) { return std::string_view(str); } };
	template<> struct FormatParamHelper<float>        { static FormatParam Make(float value           ) { return float(value)         ; } };

	template<class Thing> requires std::integral<Thing>
	struct FormatParamHelper<Thing>
	{
		static FormatParam Make(Thing value)
		{
			return int64_t(value);
		}
	};

	struct Translation
	{
		const char *name = nullptr;
		FormatterHolder *formatterHolder = nullptr;

		FormatterHolder &GetFormatterHolder()
		{
			if (!formatterHolder) [[unlikely]]
			{
				formatterHolder = &Lang::GetFormatterHolder(name);
			}
			return *formatterHolder;
		}

		void SetName(const char *newName)
		{
			name = newName;
			formatterHolder = nullptr;
		}

		std::string Format(std::span<FormatParam> params);

		template<class ...Args>
		std::string operator ()(Args &&...args)
		{
			std::array<FormatParam, sizeof...(Args)> params{{ FormatParamHelper<std::decay_t<Args>>::Make(std::forward<Args>(args))... }};
			return Format(params);
		}
	};
	template<auto Name>
	Translation &GetTranslation()
	{
		static Translation tr{ Name.chars, nullptr };
		return tr;
	}
}

template<Powder::Lang::TranslationNameHolder Name>
Powder::Lang::Translation &operator ""_St()
{
	return Powder::Lang::GetTranslation<Name>();
}
