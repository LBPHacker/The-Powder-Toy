#include "Assert.hpp"
#include "Log.hpp"

namespace Powder
{
	[[noreturn]] void AssertFail(const char *file, int line, const char *expr)
	{
		Die(file, ":", line, ": ", expr, " evaluated to false");
	}
}
