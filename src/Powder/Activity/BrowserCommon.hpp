#pragma once
#include "Gui/Colors.hpp"
#include "Gui/View.hpp"
#include "SimulationConfig.h"

namespace Powder::Activity
{
	namespace
	{
		struct SaveButtonColors
		{
			Rgba8 edge;
			Rgba8 title;
			Rgba8 author;
		};
		constexpr SaveButtonColors saveButtonColorsIdle = {
			0xFFB4B4B4_argb,
			0xFFB4B4B4_argb,
			0xFF6482A0_argb,
		};
		constexpr SaveButtonColors saveButtonColorsHovered = {
			0xFFD2E6FF_argb,
			0xFFFFFFFF_argb,
			0xFFC8E6FF_argb,
		};

		constexpr Gui::View::Size2 GetSmallThumbnailSize()
		{
			return { 105, 66 };
		}

		constexpr Gui::View::Size2 GetOriginalThumbnailSize()
		{
			return RES / 3;
		}

		constexpr Gui::View::Size2 GetSaveButtonSize()
		{
			return GetSmallThumbnailSize() + Gui::View::Size2{ 6, 24 };
		}
	}
}
