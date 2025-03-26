#pragma once
#include <sstream>
#include <cstdlib>
#include <utility>

namespace Powder
{
	void LogOne(const std::string &msg);

	template<class ...Args>
	void Log(Args &&...args)
	{
		std::ostringstream ss;
		[[maybe_unused]] auto unused = std::initializer_list<int>{ (ss << args, 0)... };
		ss << "\n";
		LogOne(ss.str());
	}

	template<class ...Args>
	[[noreturn]] void Die(Args &&...args)
	{
		Log(std::forward<Args>(args)...);
		abort();
	}
}
