#include "Translation.hpp"
#include "Language.hpp"

namespace Powder::Lang
{
	FormatterHolder &GetFormatterHolder(const char *name)
	{
		return Language::Ref().GetFormatterHolder(name);
	}

	std::string Translation::Format(std::span<FormatParam> params)
	{
		return GetFormatterHolder().formatter->Format(params);
	}
}
