#include "TargetAnimation.hpp"
#include "Host.hpp"
#include "View.hpp"

namespace Powder::Gui
{
	using BiasedTicks = Ticks;

	template<class Number>
	void TargetAnimation<Number>::SetTarget(Number newTarget)
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
	Number TargetAnimation<Number>::GetTarget() const
	{
		return target;
	}

	template<class Number>
	void TargetAnimation<Number>::SetProfile(Profile newProfile)
	{
		profile = newProfile;
	}

	template<class Number>
	void TargetAnimation<Number>::SetValue(Number newValue)
	{
		referenceTick = host.GetLastTick();
		referenceValue = newValue;
	}

	template<class Number>
	Number TargetAnimation<Number>::GetValue() const
	{
		constexpr auto tickBias = Number(1000);
		auto nowTick = host.GetLastTick();
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
			auto maxDiffTick = BiasedTicks(GetDistance() * tickBias / change);
			if (diffTick >= maxDiffTick)
			{
				return target;
			}
			return referenceValue + diffTick * change / tickBias;
		}
		if (auto *exponentialProfile = std::get_if<ExponentialProfile>(&profile))
		{
			auto distance = GetDistance();
			auto absDistance = std::abs(distance);
			if (GetReachedTarget())
			{
				return target;
			}
			auto maxDiffTick = BiasedTicks(Number(std::log(exponentialProfile->margin / absDistance) / std::log(exponentialProfile->decay) * tickBias));
			if (diffTick >= maxDiffTick)
			{
				return target;
			}
			return target - distance * std::pow(exponentialProfile->decay, diffTick / tickBias);
		}
		return Number(0);
	}

	template<class Number>
	Number TargetAnimation<Number>::GetDistance() const
	{
		if (std::holds_alternative<LinearProfile>(profile))
		{
			return target - referenceValue;
		}
		if (auto *exponentialProfile = std::get_if<ExponentialProfile>(&profile))
		{
			auto distance = target - referenceValue;
			auto absDistance = std::abs(distance);
			if (exponentialProfile->margin >= absDistance)
			{
				return Number(0);
			}
			return distance;
		}
		return Number(0);
	}

	template<class Number>
	bool TargetAnimation<Number>::GetReachedTarget() const
	{
		if (std::holds_alternative<LinearProfile>(profile))
		{
			return target == GetValue();
		}
		if (auto *exponentialProfile = std::get_if<ExponentialProfile>(&profile))
		{
			return exponentialProfile->margin >= std::abs(GetDistance());
		}
		return true;
	}

	template class TargetAnimation<int32_t>;
	template class TargetAnimation<float>;
}
