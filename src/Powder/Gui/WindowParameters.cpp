#include "WindowParameters.hpp"

namespace Powder::Gui
{
	Point WindowParameters::GetScaledWindowSize() const
	{
		int32_t scale = frameType == FrameType::fixed ? fixedScale : 1;
		return { windowSize.X * scale, windowSize.Y * scale };
	}

	std::optional<WindowParameters::Recreate> WindowParameters::Compare(const WindowParameters &other) const
	{
		if (*this == other)
		{
			return std::nullopt;
		}
		Recreate recreate;
		recreate.renderer = false;
		recreate.window = false;
		return recreate;
	}
}
