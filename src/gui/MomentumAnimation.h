#pragma once

#include <cstdint>

namespace gui
{
	class MomentumAnimation
	{
		int initialPosition = 0;
		float initialMomentum = 0.f;
		int32_t initialTime = 0;
		float halfLife = 0.f;
		float smallThreshold = 0.f;
		float limit = 0.f;

	public:
		void Start(int position, float momentum, int32_t time);
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
