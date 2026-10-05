#pragma once
#include "common/ExplicitSingleton.h"
#include "Common/CrossFrameItemStore.hpp"
#include "Common/NoCopy.hpp"
#include "Common/Format.hpp"
#include "Translation.hpp"
#include <map>
#include <memory>
#include <span>
#include <string>
#include <variant>

namespace Powder::Lang
{
	using FormatParam = std::variant<
		std::string_view,
		int64_t,
		float
	>;

	class Formatter
	{
	public:
		virtual ~Formatter() = default;

		virtual std::string Format(std::span<FormatParam> params) = 0;
	};

	struct FormatterHolder
	{
		std::unique_ptr<Formatter> formatter;
	};

	class Language;

	template<class Arg>
	struct TranslationFilter
	{
		Language &language;

		auto operator ()(auto arg)
		{
			return arg;
		}
	};

	class Language : public NoCopy, public ExplicitSingleton<Language>
	{
		std::map<std::string, FormatterHolder> formatterHolders;

		void Load(std::span<const char> data);

	public:
		Language();

		// TODO-REDO_UI: intern somewhere
		std::string Format(Translation &templ);
		std::string Format1s(Translation &templ, const ByteString &s);

		template<class ...Args>
		std::string BuildString(Args &&...args);

		FormatterHolder &GetFormatterHolder(const char *name);
	};

	template<>
	struct TranslationFilter<Translation>
	{
		Language &language;

		auto operator ()(Translation &arg)
		{
			return language.Format(arg);
		}
	};

	template<class ...Args>
	std::string Language::BuildString(Args &&...args)
	{
		return Powder::BuildString(TranslationFilter<std::decay_t<Args>>{ *this }(std::forward<Args>(args))...);
	}
}
