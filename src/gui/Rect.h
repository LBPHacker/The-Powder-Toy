#pragma once

#include "Point.h"

namespace gui
{
	struct Rect
	{
		Point pos, size;

		bool Contains(Point point) const
		{
			return point.x >= pos.x && point.y >= pos.y && point.x < pos.x + size.x && point.y < pos.y + size.y;
		}

		Rect Offset(Point offset) const
		{
			Rect result = *this;
			result.pos += offset;
			return result;
		}

		static Rect FromCorners(Point one, Point other)
		{
			auto lx = one.x < other.x ? one.x : other.x;
			auto ly = one.y < other.y ? one.y : other.y;
			auto hx = one.x > other.x ? one.x : other.x;
			auto hy = one.y > other.y ? one.y : other.y;
			return Rect{ Point{ lx, ly }, Point{ hx - lx + 1, hy - ly + 1 } };
		}

		Point TopLeft() const
		{
			return pos;
		}

		Point TopRight() const
		{
			return pos + Point{ size.x - 1, 0 };
		}

		Point BottomLeft() const
		{
			return pos + Point{ 0, size.y - 1 };;
		}

		Point BottomRight() const
		{
			return pos + Point{ size.x - 1, size.y - 1 };
		}

		Point Clamp(Point point) const
		{
			auto tl = TopLeft();
			auto br = BottomRight();
			if (point.x < tl.x) point.x = tl.x;
			if (point.y < tl.y) point.y = tl.y;
			if (point.x > br.x) point.x = br.x;
			if (point.y > br.y) point.y = br.y;
			return point;
		}

		Rect Intersect(Rect other) const
		{
			return FromCorners(Clamp(other.TopLeft()), Clamp(other.BottomRight()));
		}
	};
}
