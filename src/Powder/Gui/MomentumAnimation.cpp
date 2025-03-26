#include "MomentumAnimation.hpp"
#include "Host.hpp"
#include "View.hpp"

namespace Powder::Gui
{
	using BiasedTicks = Ticks;

	template<class Number>
	void MomentumAnimation<Number>::SetMomentum(Number newMomentum)
	{
		auto value = GetValue();
		Momentum::SetValue(newMomentum);
		SetValue(value);
	}

	template<class Number>
	Number MomentumAnimation<Number>::GetMomentum() const
	{
		return Momentum::GetValue();
	}

	template<class Number>
	void MomentumAnimation<Number>::SetProfile(Profile newProfile)
	{
		Momentum::SetProfile(newProfile);
	}

	template<class Number>
	void MomentumAnimation<Number>::SetValue(Number newValue)
	{
		referenceValue = newValue;
	}

	template<class Number>
	Number MomentumAnimation<Number>::GetValue() const
	{
		constexpr auto tickBias = Number(1000);
		auto nowTick = Momentum::host.GetLastTick();
		auto diffTick = nowTick - Momentum::referenceTick;
		if (auto *exponentialProfile = std::get_if<ExponentialProfile>(&(TargetAnimation<Number>::profile)))
		{
			auto reachedTarget = Momentum::GetReachedTarget();
			if (reachedTarget)
			{
				return referenceValue;
			}
			auto distance = Momentum::GetDistance();
			auto maxDiffTick = BiasedTicks(Number(std::log(exponentialProfile->margin / std::abs(distance)) / std::log(exponentialProfile->decay) * tickBias));
			auto momentum = Number(0);
			if (diffTick >= maxDiffTick)
			{
				if (!reachedTarget)
				{
					momentum = std::copysign(exponentialProfile->margin, -distance);
				}
			}
			else
			{
				momentum = GetMomentum();
			}
			return referenceValue + (momentum + distance) / std::log(exponentialProfile->decay);
		}
		return Number(0);
	}

	template class MomentumAnimation<float>;
}
