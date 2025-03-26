#pragma once
#include <cstdint>

namespace Powder::Gui
{
	struct CommonMetrics
	{
		int32_t size;
		int32_t padding;
		int32_t spacing;
		int32_t smallButton;
		int32_t bigButton;
		int32_t hugeButton;
		int32_t smallSpinner;
		int32_t bigSpinner;
		int32_t hugeSpinner;
		int32_t heightToFitSize;

		CommonMetrics();
	};
}
