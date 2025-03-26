#include "Animation.hpp"
#include "Host.hpp"

namespace Powder::Gui
{
	using BiasedTicks = Ticks;

	template<class Number>
	void Animation<Number>::SetTarget(Number newTarget)
	{
		if (target == newTarget)
		{
			return;
		}
		auto value = GetValue();
		target = newTarget;
		SetValue(value);
	}

	template<class Number>
	void Animation<Number>::SetProfile(Profile newProfile)
	{
		profile = newProfile;
	}

	template<class Number>
	void Animation<Number>::SetValue(Number newValue)
	{
		referenceTick = Host::Ref().GetLastTick();
		referenceValue = newValue;
	}

	template<class Number>
	Number Animation<Number>::GetValue() const
	{
		constexpr auto tickBias = Number(1000);
		auto nowTick = Host::Ref().GetLastTick();
		auto diffTick = nowTick - referenceTick;
		if (auto *linearProfile = std::get_if<LinearProfile>(&profile))
		{
			auto change = linearProfile->change;
			if (target < referenceValue)
			{
				if (linearProfile->changeDownward)
				{
					change = *linearProfile->changeDownward;
				}
				change = -change;
			}
			auto maxDiffTick = BiasedTicks((target - referenceValue) * tickBias / change);
			if (diffTick >= maxDiffTick)
			{
				return target;
			}
			return referenceValue + diffTick * change / tickBias;
		}
		if (auto *exponentialProfile = std::get_if<ExponentialProfile>(&profile))
		{
			auto maxDiffTick = BiasedTicks(Number(std::log(exponentialProfile->margin / Number(std::abs(referenceValue - target))) / Number(std::log(exponentialProfile->decay)) * tickBias));
			if (diffTick >= maxDiffTick)
			{
				return target;
			}
			return target + (referenceValue - target) * Number(std::pow(exponentialProfile->decay, diffTick / tickBias));
		}
		return Number(0);
	}

	template class Animation<int32_t>;
	template class Animation<float>;
}
