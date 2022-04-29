#pragma once
#include "Config.h"

#include "Timestamp.h"

namespace gui
{
	class MomentumAnimation
	{
		int initialPosition = 0;
		float initialMomentum = 0.f;
		Timestamp initialTime = 0;
		float halfLife = 0.f;
		float smallThreshold = 0.f;
		float limit = 0.f;

	public:
		void Start(int position, float momentum);
		int EffectivePosition() const;
		float EffectiveMomentum() const;
		bool Small() const;
		void Cancel(int position);
		bool CancelIfSmall();
		void Accelerate(float momentumDiff);

		void HalfLife(float newHalfLife);
		float HalfLife() const
		{
			return halfLife;
		}

		void SmallThreshold(float newSmallThreshold);
		float SmallThreshold() const
		{
			return smallThreshold;
		}

		void Limit(float newLimit);
		float Limit() const
		{
			return limit;
		}
	};
}
