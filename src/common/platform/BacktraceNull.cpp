#include "Platform.h"

namespace Platform
{
std::optional<std::vector<String>> Backtrace(BacktraceType)
{
	return std::nullopt;
}
}
