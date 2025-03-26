#pragma once
#include "View.hpp"

#if DebugGuiView
# include "Common/Log.hpp"
#endif
#define DebugGuiViewStats 0
#define DebugGuiViewDump 0
#define DebugGuiViewChildren 0
#define DebugGuiViewRects 0

namespace Powder::Gui
{
	namespace
	{
		constexpr View::Pos maxPos   = 100'000;
		constexpr View::Size maxSize = 100'000;
	}

	inline View::Pos &operator %(View::Pos2 &point, View::AxisBase index)
	{
		return index ? point.Y : point.X;
	}

	template<class Item>
	Item &operator %(View::ExtendedSize2<Item> &point, View::AxisBase index)
	{
		return index ? point.Y : point.X;
	}

	inline void ClampSize(View::Size &size)
	{
		size = std::clamp(size, 0, maxSize);
	}

	inline void ClampPos(View::Pos &pos)
	{
		pos = std::clamp(pos, -maxPos, maxPos);
	}

	inline void ClampSize(View::Size2 &size)
	{
		ClampSize(size.X);
		ClampSize(size.Y);
	}

	inline void ClampPos(View::Size2 &size)
	{
		ClampPos(size.X);
		ClampPos(size.Y);
	}

	template<class SizeHolder>
	void ClampSize(SizeHolder &sizeHolder)
	{
		if (auto *size = std::get_if<View::Size>(&sizeHolder))
		{
			ClampSize(*size);
		}
	}

	template<class>
	struct DependentFalse : std::false_type
	{
	};
}
