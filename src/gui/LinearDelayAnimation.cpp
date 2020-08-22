#include "LinearDelayAnimation.h"

#include "SDLWindow.h"

namespace gui
{
	void LinearDelayAnimation::Start(float value, int32_t time)
	{
		initialValue = value;
		initialTime = time;
	}

	float LinearDelayAnimation::Current() const
	{
		float effective = initialValue + (up ? 1 : -1) * slope * (SDLWindow::Ticks() - initialTime) / 1000.f;
		if (effective > upperLimit) effective = upperLimit;
		if (effective < lowerLimit) effective = lowerLimit;
		return effective;
	}

	void LinearDelayAnimation::Up()
	{
		if (!up)
		{
			Start(Current(), SDLWindow::Ticks());
			up = true;
		}
	}

	void LinearDelayAnimation::Down()
	{
		if (up)
		{
			Start(Current(), SDLWindow::Ticks());
			up = false;
		}
	}

	void LinearDelayAnimation::Slope(float newSlope)
	{
		slope = newSlope;
		Start(Current(), SDLWindow::Ticks());
	}
}
