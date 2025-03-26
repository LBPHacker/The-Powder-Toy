#pragma once
#include "TargetAnimation.hpp"

namespace Powder::Gui
{
	template<class Number>
	class MomentumAnimation : private TargetAnimation<Number>
	{
		using Momentum = TargetAnimation<Number>;

		Number referenceValue = 0;

	public:
		using LinearProfile      = Momentum::LinearProfile     ;
		using ExponentialProfile = Momentum::ExponentialProfile;
		using Profile            = Momentum::Profile           ;

		MomentumAnimation(Host &newHost, Profile newProfile, Number newMomentum, Number newValue) : Momentum(newHost, newProfile, Number(0), newMomentum)
		{
			SetValue(newValue);
		}

		MomentumAnimation(Host &newHost, Profile newProfile = {}, Number newValue = Number(0)) : MomentumAnimation(newHost, newProfile, Number(0), newValue)
		{
		}

		void SetMomentum(Number newMomentum);
		Number GetMomentum() const;
		void SetProfile(Profile newProfile);
		void SetValue(Number newValue);
		Number GetValue() const;
	};
}
