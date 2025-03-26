#pragma once
#include "common/String.h"
#include <ctime>

namespace Powder
{
	ByteString FormatRelative(time_t time, time_t now);

	template<class ...Args>
	inline std::string BuildString(Args &&...args)
	{
		std::ostringstream ss;
		[[maybe_unused]] auto unused = std::initializer_list<int>{ (ss << std::forward<Args>(args), 0)... };
		return ss.str();
	}
}
