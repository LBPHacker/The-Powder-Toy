#include "SdlAssert.hpp"
#include "Common/Log.hpp"

namespace Powder::Gui
{
	[[noreturn]] void SdlAssertFail(const char *file, int line, const char *expr)
	{
		Die(file, ":", line, ": ", expr, " evaluated to false; SDL_GetError(): ", SDL_GetError());
	}
}
