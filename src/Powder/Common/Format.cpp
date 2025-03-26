#include "Format.hpp"
#include "Lang/Translation.hpp"
#include <vector>
#include <cstdint>

namespace Powder
{
	ByteString FormatRelative(time_t time, time_t now)
	{
		auto diff = int64_t(difftime(now, time));
		struct Unit
		{
			Lang::Translation &tr;
			int64_t seconds;
		};
		const std::vector<Unit> units = {
			{ "DEFAULT_LANG_FORMAT_TIME_YEARS"_St  , 31556736 },
			{ "DEFAULT_LANG_FORMAT_TIME_MONTHS"_St ,  2592000 },
			{ "DEFAULT_LANG_FORMAT_TIME_WEEKS"_St  ,   604800 },
			{ "DEFAULT_LANG_FORMAT_TIME_DAYS"_St   ,    86400 },
			{ "DEFAULT_LANG_FORMAT_TIME_HOURS"_St  ,     3600 },
			{ "DEFAULT_LANG_FORMAT_TIME_MINUTES"_St,       60 },
			{ "DEFAULT_LANG_FORMAT_TIME_SECONDS"_St,        1 },
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
			return "DEFAULT_LANG_FORMAT_TIME_JUSTNOW"_St();
		}
		std::array<Lang::FormatParam, 1> params{{ countUsed }};
		return unitUsed->tr.Format(params);
	}
}
