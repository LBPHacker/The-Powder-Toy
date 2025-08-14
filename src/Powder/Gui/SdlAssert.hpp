#pragma once
#include "Common/Assert.hpp"
#include <SDL.h>

namespace Powder::Gui
{
	[[noreturn]] void SdlAssertFail(const char *file, int line, const char *expr);

	inline void SdlAssertZero(int value, const char *file, int line, const char *expr)
	{
		if (!(value == 0))
		{
			SdlAssertFail(file, line, expr);
		}
	}
#define SdlAssertZero(a) (::Powder::Gui::SdlAssertZero((a), __FILE__, __LINE__, #a " == 0"))

	inline void SdlAssertTrue(SDL_bool value, const char *file, int line, const char *expr)
	{
		if (!(value == SDL_TRUE))
		{
			SdlAssertFail(file, line, expr);
		}
	}
#define SdlAssertTrue(a) (::Powder::Gui::SdlAssertTrue((a), __FILE__, __LINE__, #a " == SDL_TRUE"))

	template<class Ptr>
	Ptr SdlAssertPtr(Ptr value, const char *file, int line, const char *expr)
	{
		if (!(value != nullptr))
		{
			SdlAssertFail(file, line, expr);
		}
		return value;
	}
#define SdlAssertPtr(a) (::Powder::Gui::SdlAssertPtr((a), __FILE__, __LINE__, #a " != nullptr"))
}
