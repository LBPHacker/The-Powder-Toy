#include "Log.hpp"
#include "Gui/SdlAssert.hpp"
#include <iostream>

namespace Powder
{
	void LogOne(const std::string &msg)
	{
		SDL_Log("%s\n", msg.c_str());
	}
}
