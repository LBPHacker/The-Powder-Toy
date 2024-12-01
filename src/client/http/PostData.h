#pragma once
#include "common/String.h"
#include <optional>
#include <variant>
#include <vector>

namespace http
{
struct Header
{
	ByteString name;
	ByteString value;
};

struct FormItem
{
	ByteString name;
	ByteString value;
	std::optional<ByteString> filename;
};

using StringData = ByteString;
using FormData = std::vector<FormItem>;
using PostData = std::variant<StringData, FormData>;
};
