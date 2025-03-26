#pragma once
#include "Translation.hpp"
#include "Gui/Text.hpp"
#include "common/String.h"
#include <string>
#include <variant>

namespace Powder::Gui
{
	class Host;
}

namespace Powder::Lang
{
	struct StringView
	{
		using Held = std::variant<
			std::string_view,
			Translation *
		>;
		Held held;

		StringView() = default;

		template<size_t N> // TODO-REDO_UI: remove
		StringView(const char (&arr)[N]) : held(std::string_view(arr, N - 1))
		{
			static_assert(N > 0);
		}

		StringView(Translation &templ) : held(&templ)
		{
		}

		StringView(std::string_view view) : held(view)
		{
		}

		StringView(const std::string &str) : held(std::string_view(str))
		{
		}

		StringView(const ByteString &str) : held(std::string_view(str))
		{
		}

		Gui::InternedTextIndex Materialize(Gui::Host &host) const;
		bool Empty() const;
	};
}
