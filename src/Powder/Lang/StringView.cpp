#include "StringView.hpp"
#include "Language.hpp"
#include "Gui/Host.hpp"

namespace Powder::Lang
{
	Gui::InternedTextIndex StringView::Materialize(Gui::Host &host) const
	{
		if (auto *view = std::get_if<std::string_view>(&held))
		{
			return host.InternText(*view);
		}
		return host.InternText(Lang::Language::Ref().BuildString(*std::get<Translation *>(held)));
	}

	bool StringView::Empty() const
	{
		if (auto *view = std::get_if<std::string_view>(&held))
		{
			return view->empty();
		}
		return false; // n.b. a language template should never be empty
	}
}
