#pragma once
#include "graphics/Pixel.h"
#include <cstdint>

namespace Powder
{
	// TODO-REDO_UI-POSTCLEANUP: move definition here
	using Rgba8 = ::RGBA;
	using Rgb8 = ::RGB;

	constexpr Rgb8 GetContrastColor(Rgb8 color)
	{
		return (2 * color.Red + 3 * color.Green + color.Blue < 512) ? 0xC0C0C0_rgb : 0x404040_rgb;
	}

	constexpr Rgb8 GetContrastColor2(Rgb8 color)
	{
		return (2 * color.Red + 3 * color.Green + color.Blue < 544) ? 0xFFFFFF_rgb : 0x000000_rgb;
	}

	constexpr Rgb8 InvertColor(Rgb8 color)
	{
		return Rgb8(255 - color.Red, 255 - color.Green, 255 - color.Blue);
	}
}
