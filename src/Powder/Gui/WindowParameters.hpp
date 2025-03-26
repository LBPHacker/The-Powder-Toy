#pragma once
#include "Common/Point.hpp"
#include <cstdint>
#include <optional>

namespace Powder::Gui
{
	struct WindowParameters
	{
		Point windowSize = { 629, 424 };

		enum class FrameType
		{
			fixed,
			resizable,
			fullscreen,
		};
		FrameType frameType = FrameType::fixed;
		int32_t fixedScale = 1;
		bool fullscreenChangeResolution  = false;
		bool fullscreenForceIntegerScale = true;
		bool blurryScaling = false;

		bool operator ==(const WindowParameters &) const = default;

		struct Recreate
		{
			bool renderer;
			bool window;
		};
		std::optional<Recreate> Compare(const WindowParameters &other) const;

		Point GetScaledWindowSize() const;
		void SetPrefs() const;
	};
}
