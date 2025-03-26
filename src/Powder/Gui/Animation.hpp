#pragma once
#include "Ticks.hpp"
#include <optional>
#include <variant>

namespace Powder::Gui
{
	template<class Number>
	class Animation
	{
	public:
		struct LinearProfile
		{
			Number change; // per second
			std::optional<Number> changeDownward; // per second, ::change is upward change if set
		};
		struct ExponentialProfile
		{
			Number decay; // per second
			Number margin; // unit
		};
		using Profile = std::variant<
			LinearProfile,
			ExponentialProfile
		>;

	private:
		Number target = 0;
		Ticks referenceTick = 0;
		Number referenceValue = 0;
		Profile profile;

	public:
		Animation(Profile newProfile, Number newTarget, Number newValue)
		{
			SetProfile(newProfile);
			SetTarget(newTarget);
			SetValue(newValue);
		}

		Animation(Profile newProfile = {}, Number newTarget = Number(0)) : Animation(newProfile, newTarget, newTarget)
		{
		}

		void SetTarget(Number newTarget);
		void SetProfile(Profile newProfile);
		void SetValue(Number newValue);
		Number GetValue() const;
	};
}
