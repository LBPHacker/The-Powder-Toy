#include "LinearDelayAnimation.h"

#include "SDLWindow.h"

namespace gui
{
	void LinearDelayAnimation::Start(float value, bool newUp)
	{
		initialValue = value;
		initialTime = SDLWindow::Ref().EventTimestamp();
		up = newUp;
	}

	float LinearDelayAnimation::Current() const
	{
		float effective = initialValue + (up ? 1 : -1) * slope * (SDLWindow::Ref().EventTimestamp() - initialTime) / 1000.f;
		if (effective > upperLimit) effective = upperLimit;
		if (effective < lowerLimit) effective = lowerLimit;
		return effective;
	}

	void LinearDelayAnimation::Up()
	{
		if (!up)
		{
			Start(Current(), true);
		}
	}

	void LinearDelayAnimation::Down()
	{
		if (up)
		{
			Start(Current(), false);
		}
	}

	void LinearDelayAnimation::Slope(float newSlope)
	{
		slope = newSlope;
		Start(Current(), up);
	}
}
