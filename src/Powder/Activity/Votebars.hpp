#pragma once
#include "Gui/Colors.hpp"
#include <algorithm>
#include <cstdint>
#include <utility>

namespace Powder::Activity
{
	constexpr Rgba8 voteUpBackground       = 0xFF006B0A_argb;
	constexpr Rgba8 voteDownBackground     = 0xFF6B0A00_argb;
	constexpr Rgba8 voteUpBar              = 0xFF39BB39_argb;
	constexpr Rgba8 voteDownBar            = 0xFFBB3939_argb;
	constexpr int32_t voteBarFillThreshold = 8;

	inline std::pair<int32_t, int32_t> ScaleVoteBars(int32_t maxSizeUp, int32_t maxSizeDown, int32_t scoreUp, int32_t scoreDown)
	{
		int32_t sizeUp = 0;
		int32_t sizeDown = 0;
		auto scaleMax = std::max(scoreUp, scoreDown);
		if (scaleMax)
		{
			if (scaleMax < voteBarFillThreshold)
			{
				scaleMax *= voteBarFillThreshold - scaleMax;
			}
			sizeUp   = scoreUp   * maxSizeUp   / scaleMax;
			sizeDown = scoreDown * maxSizeDown / scaleMax;
		}
		return { sizeUp, sizeDown };
	}
}
