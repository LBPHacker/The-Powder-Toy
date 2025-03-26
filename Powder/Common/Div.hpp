#pragma once

namespace Powder
{
	template<class Integer>
	constexpr Integer FloorDiv(Integer a, Integer b)
	{
		// [expr.mul] For integral operands the / operator yields the algebraic quotient with any fractional part discarded, [...]
		auto q = a / b;
		auto r = a % b;
		auto s = a ^ b;
		if (s < 0 && r)
		{
			return --q;
		}
		return q;
	}

	template<class Integer>
	constexpr Integer TruncateDiv(Integer a, Integer b)
	{
		// [expr.mul] For integral operands the / operator yields the algebraic quotient with any fractional part discarded, [...]
		return a / b;
	}

	template<class Integer>
	constexpr Integer FloorMod(Integer a, Integer b)
	{
		return a - FloorDiv(a, b) * b;
	}
}
