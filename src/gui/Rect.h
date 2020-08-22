#pragma once

#include "Point.h"

namespace gui
{
	struct Rect
	{
		Point pos, size;

		Rect Intersect(Rect other) const
		{
			auto ctl = pos;
			auto csz = size;
			auto &ntl = other.pos;
			auto &nsz = other.size;
			auto cbr = ctl + csz;
			auto nbr = ntl + nsz;
			if (ntl.x < ctl.x) ntl.x = ctl.x;
			if (ntl.y < ctl.y) ntl.y = ctl.y;
			if (nbr.x > cbr.x) nbr.x = cbr.x;
			if (nbr.y > cbr.y) nbr.y = cbr.y;
			if (nbr.x < ntl.x) nbr.x = ntl.x;
			if (nbr.y < ntl.y) nbr.y = ntl.y;
			nsz = nbr - ntl;
			return other;
		}

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
	};
}
