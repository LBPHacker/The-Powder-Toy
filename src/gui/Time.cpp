#include "Time.h"

#include "SDLWindow.h"

#include <cstdint>

namespace gui
{
namespace Time
{
	static_assert(sizeof(time_t) >= sizeof(int64_t), "get a real OS");

	time_t Now()
	{
		return time(NULL);
	}

	struct tm LocalTime(time_t time)
	{
		struct tm ltm;
#if (_POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _BSD_SOURCE || _SVID_SOURCE || _POSIX_SOURCE) || defined(MACOSX)
		localtime_r(&time, &ltm);
#elif defined(WIN)
		// * The functions in this family are actually thread-safe on windows (including mingw), huh:
		//   https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/localtime-localtime32-localtime64?view=msvc-160
		//   https://stackoverflow.com/questions/18551409/localtime-r-support-on-mingw
		auto *tlsLtm = localtime(&time);
		if (tlsLtm)
		{
			ltm = *tlsLtm;
		}
#else
# error That is unfortunate.
#endif
		return ltm;
	}

	String FormatAbsolute(time_t time)
	{
		char buf[100];
		auto ltm = LocalTime(time);
		strftime(buf, sizeof(buf), SDLWindow::Ref().TimeFormat().ToUtf8().c_str(), &ltm);
		return ByteString(buf).FromUtf8();
	}

	String FormatRelative(time_t time, time_t now)
	{
		auto diff = int64_t(difftime(now, time));
		struct Unit
		{
			String one;
			String more;
			int64_t seconds;
		};
		// * TODO-REDO_UI: Localize.
		const std::vector<Unit> units = {
			{   "a year ago",   " years ago", 31556736 },
			{  "a month ago",  " months ago",  2592000 },
			{   "a week ago",   " weeks ago",   604800 },
			{    "a day ago",    " days ago",    86400 },
			{  "an hour ago",   " hours ago",     3600 },
			{ "a minute ago", " minutes ago",       60 },
			{ "a second ago", " seconds ago",        1 },
		};
		const Unit *unitUsed = nullptr;
		int64_t countUsed = 0;
		for (auto &unit : units)
		{
			auto count = diff / unit.seconds;
			if (count >= 1)
			{
				countUsed = count;
				unitUsed = &unit;
				break;
			}
		}
		if (!unitUsed)
		{
			return "???";
		}
		if (countUsed == 1)
		{
			return unitUsed->one;
		}
		return String::Build(countUsed, unitUsed->more);
	}
}
}