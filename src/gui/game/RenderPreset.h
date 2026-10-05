#pragma once
#include "common/String.h"
#include "graphics/RendererSettings.h"
#include <cstdint>

namespace Powder::Lang
{
	struct Translation;
}

struct RenderPreset
{
	Powder::Lang::Translation &Name;
	uint32_t renderMode;
	uint32_t displayMode;
	uint32_t colorMode;
	HdispLimit wantHdispLimitMin = HdispLimitExplicit{ MIN_TEMP };
	HdispLimit wantHdispLimitMax = HdispLimitExplicit{ MAX_TEMP };
};
