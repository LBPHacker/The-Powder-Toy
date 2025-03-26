#pragma once
#include "Common/Color.hpp"

namespace Powder::Gui
{
	constexpr auto colorWhite       = 0xFFFFFF_rgb; // high contrast
	constexpr auto colorGray        = 0xC0C0C0_rgb; // low contrast
	constexpr auto colorYellow      = 0xFFD820_rgb; // OK buttons, spark signs
	constexpr auto colorRed         = 0xFF0000_rgb; // warnings
	constexpr auto colorLightRed    = 0xFF4B4B_rgb; // thread signs
	constexpr auto colorBlue        = 0x0000FF_rgb; // unused?
	constexpr auto colorTeal        = 0x20AAFF_rgb; // save signs
	constexpr auto colorPurple      = 0x9353D3_rgb; // search signs
	constexpr auto colorTitlePurple = 0x8C8CFF_rgb; // modal titles
}
