#pragma once
#include "common/String.h"
#include <ctime>

namespace Powder
{
	ByteString FormatRelative(time_t time, time_t now);
}
