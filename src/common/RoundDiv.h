#pragma once
#include <utility>

template<class Signed>
constexpr std::pair<Signed, Signed> floorDiv(Signed a, Signed b)
{
	auto quo = a / b;
	auto rem = a % b;
	if (a < Signed(0) && rem)
	{
		quo -= Signed(1);
		rem += b;
	}
	return { quo, rem };
}

template<class Signed>
constexpr std::pair<Signed, Signed> ceilDiv(Signed a, Signed b)
{
	return floorDiv(a + b - Signed(1), b);
}
