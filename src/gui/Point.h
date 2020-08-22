#pragma once

namespace gui
{
	struct Point
	{
		int x, y;

		Point operator +(Point other) const
		{
			return Point{ x + other.x, y + other.y };
		}

		Point operator -(Point other) const
		{
			return *this + other * -1;
		}

		Point operator *(int scale) const
		{
			return Point{ x * scale, y * scale };
		}

		Point operator /(int scale) const
		{
			return Point{ x / scale, y / scale };
		}

		Point &operator +=(Point other)
		{
			*this = *this + other;
			return *this;
		}

		Point &operator -=(Point other)
		{
			*this = *this - other;
			return *this;
		}

		Point &operator *=(int scale)
		{
			*this = *this * scale;
			return *this;
		}

		Point &operator /=(int scale)
		{
			*this = *this / scale;
			return *this;
		}

		bool operator ==(Point other) const
		{
			return x == other.x && y == other.y;
		}

		bool operator !=(Point other) const
		{
			return !(*this == other);
		}

		friend Point operator *(int scale, Point point)
		{
			return point * scale;
		}
	};
}
