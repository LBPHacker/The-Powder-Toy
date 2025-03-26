#pragma once
#include <cstdint>
#include <string>
#include <utility>

namespace Powder::Lang
{
	class Format
	{
		void InitOne(std::string_view str);
		void InitOne(int64_t num);

		template<class Thing, class ...Rest>
		void InitPart(Thing &&thing, Rest &&...rest)
		{
			InitOne(thing);
			InitPart(std::forward<Rest>(rest)...);
		}

	public:
		template<class ...Args>
		Format(std::string_view templateName, Args &&...args)
		{
			InitOne(templateName);
			InitPart(std::forward<Args>(args)...);
		}
	};
}
