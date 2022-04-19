#pragma once

#include <array>

namespace gui
{
	template<class Type>
	struct XPoint
	{
		Type x, y;

		constexpr XPoint operator +(XPoint other) const
		{
			return { x + other.x, y + other.y };
		}

		constexpr XPoint operator -(XPoint other) const
		{
			return *this + other * -1;
		}

		constexpr XPoint operator *(Type scale) const
		{
			return { x * scale, y * scale };
		}

		constexpr XPoint operator /(Type scale) const
		{
			return { x / scale, y / scale };
		}

		constexpr XPoint operator -() const
		{
			return { -x, -y };
		}

		XPoint &operator +=(XPoint other)
		{
			*this = *this + other;
			return *this;
		}

		XPoint &operator -=(XPoint other)
		{
			*this = *this - other;
			return *this;
		}

		XPoint &operator *=(Type scale)
		{
			*this = *this * scale;
			return *this;
		}

		XPoint &operator /=(Type scale)
		{
			*this = *this / scale;
			return *this;
		}

		constexpr bool operator ==(XPoint other) const
		{
			return x == other.x && y == other.y;
		}

		constexpr bool operator !=(XPoint other) const
		{
			return !(*this == other);
		}

		friend constexpr XPoint operator *(Type scale, XPoint point)
		{
			return point * scale;
		}

		template<class Index>
		Type &operator [](Index index)
		{
			return index ? y : x;
		}

		template<class Index>
		const Type &operator [](Index index) const
		{
			return index ? y : x;
		}

		template<class Index>
		constexpr Type operator [](Index index) const
		{
			return index ? y : x;
		}

		constexpr XPoint Transform(const std::array<XPoint, 2> mat) const
		{
			return {
				mat[0].x * x + mat[1].x * y,
				mat[0].y * x + mat[1].y * y,
			};
		}
	};

	using Point = XPoint<int>;
	using FPoint = XPoint<float>;
}
