#pragma once

namespace Powder
{
	[[noreturn]] void AssertFail(const char *file, int line, const char *expr);

	inline void Assert(bool ok, const char *file, int line, const char *expr)
	{
		if (!ok)
		{
			AssertFail(file, line, expr);
		}
	}
#define Assert(a) (::Powder::Assert(a, __FILE__, __LINE__, #a))
}
