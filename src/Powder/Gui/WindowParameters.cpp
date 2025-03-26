#include "WindowParameters.hpp"
#include "prefs/GlobalPrefs.h"

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

	void WindowParameters::SetPrefs() const
	{
		auto &prefs = GlobalPrefs::Ref();
		GlobalPrefs::DeferWrite dw(prefs);
		prefs.Set("Resizable"          , frameType == Gui::WindowParameters::FrameType::resizable );
		prefs.Set("Fullscreen"         , frameType == Gui::WindowParameters::FrameType::fullscreen);
		prefs.Set("AltFullscreen"      , fullscreenChangeResolution                               );
		prefs.Set("ForceIntegerScaling", fullscreenForceIntegerScale                              );
		prefs.Set("Scale"              , fixedScale                                               );
		prefs.Set("BlurryScaling"      , blurryScaling                                            );
	}
}
