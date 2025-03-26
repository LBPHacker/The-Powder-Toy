#include "Format.hpp"
#include <vector>
#include <cstdint>

namespace Powder
{
	ByteString FormatRelative(time_t time, time_t now)
	{
		auto diff = int64_t(difftime(now, time));
		struct Unit
		{
			ByteString one;
			ByteString more;
			int64_t seconds;
		};
		const std::vector<Unit> units = { // TODO-REDO_UI-TRANSLATE
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
			return "just now"; // TODO-REDO_UI-TRANSLATE
		}
		if (countUsed == 1)
		{
			return unitUsed->one;
		}
		return ByteString::Build(countUsed, unitUsed->more);
	}
}
