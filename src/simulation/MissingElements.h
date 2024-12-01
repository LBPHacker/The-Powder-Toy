#pragma once
#include "common/String.h"
#include <map>
#include <set>

struct MissingElements
{
	std::map<ByteString, int> identifiers;
	std::set<int> ids;

	operator bool() const
	{
		return identifiers.size() || ids.size();
	}
};
