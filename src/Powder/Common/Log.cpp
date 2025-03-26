#include "Log.hpp"
#include "Gui/SdlAssert.hpp"
#include <iostream>

namespace Powder
{
	void LogOne(const std::string &msg)
	{
		SDL_Log("%s", msg.c_str());
	}
}
