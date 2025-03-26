#include "Log.hpp"
#include <iostream>

namespace Powder
{
	void LogOne(const std::string &msg)
	{
		std::cerr << msg << std::flush;
	}
}
