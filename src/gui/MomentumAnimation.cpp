#include "MomentumAnimation.h"

#include "SDLWindow.h"

#include <cmath>

namespace gui
{
	void MomentumAnimation::Start(int position, float momentum)
	{
		initialPosition = position;
		initialMomentum = momentum;
		initialTime = SDLWindow::Ref().EventTimestamp();
	}

	int MomentumAnimation::EffectivePosition() const
	{
		if (fabsf(initialMomentum) < smallThreshold)
		{
			return initialPosition;
		}
		return initialPosition + int(halfLife * 1.4427f * (initialMomentum - EffectiveMomentum())); // integrate(a * .5 ^ (t / t_{1/2}), t, 0, T)
	}

	float MomentumAnimation::EffectiveMomentum() const
	{
		return initialMomentum * exp2f(-((SDLWindow::Ref().EventTimestamp() - initialTime) / 1000.f) / halfLife); // a * .5 ^ (t / t_{1/2})
	}

	bool MomentumAnimation::Small() const
	{
		return fabsf(EffectiveMomentum()) < smallThreshold;
	}

	void MomentumAnimation::Cancel(int position)
	{
		Start(position, 0.f);
	}

	bool MomentumAnimation::CancelIfSmall()
	{
		if (fabsf(initialMomentum) >= smallThreshold && Small())
		{
			Cancel(EffectivePosition());
			return true;
		}
		return false;
	}

	void MomentumAnimation::HalfLife(float newHalfLife)
	{
		auto effectivePosition = EffectivePosition();
		auto effectiveMomentum = EffectiveMomentum();
		halfLife = newHalfLife;
		Start(effectivePosition, effectiveMomentum);
	}

	void MomentumAnimation::SmallThreshold(float newSmallThreshold)
	{
		smallThreshold = newSmallThreshold;
		CancelIfSmall();
	}

	void MomentumAnimation::Limit(float newLimit)
	{
		limit = newLimit;
		auto effectiveMomentum = EffectiveMomentum();
		if (fabsf(effectiveMomentum) > limit)
		{
			Start(EffectivePosition(), copysignf(limit, effectiveMomentum));
		}
		CancelIfSmall();
	}

	void MomentumAnimation::Accelerate(float momentumDiff)
	{
		auto effectivePosition = EffectivePosition();
		auto effectiveMomentum = EffectiveMomentum() + momentumDiff;
		if (fabsf(effectiveMomentum) > limit)
		{
			effectiveMomentum = copysignf(limit, effectiveMomentum);
		}
		Start(effectivePosition, effectiveMomentum);
		CancelIfSmall();
	}
}
