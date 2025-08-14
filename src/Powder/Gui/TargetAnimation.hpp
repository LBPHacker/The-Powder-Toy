#pragma once
#include "Ticks.hpp"
#include <optional>
#include <variant>

namespace Powder::Gui
{
	class Host;

	template<class Number>
	class TargetAnimation
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

	protected:
		Host &host;
		Number target = 0;
		Ticks referenceTick = 0;
		Number referenceValue = 0;
		Profile profile;

	public:
		TargetAnimation(Host &newHost, Profile newProfile, Number newTarget, Number newValue) : host(newHost)
		{
			SetProfile(newProfile);
			SetTarget(newTarget);
			SetValue(newValue);
		}

		TargetAnimation(Host &newHost, Profile newProfile = {}, Number newTarget = Number(0)) : TargetAnimation(newHost, newProfile, newTarget, newTarget)
		{
		}

		void SetTarget(Number newTarget);
		Number GetTarget() const;
		void SetProfile(Profile newProfile);
		void SetValue(Number newValue);
		Number GetValue() const;
		Number GetDistance() const;
		bool GetReachedTarget() const;
	};
}
