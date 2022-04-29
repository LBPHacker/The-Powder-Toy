#pragma once
#include "Config.h"

#include "Timestamp.h"

namespace gui
{
	class LinearDelayAnimation
	{
		Timestamp initialTime = 0;
		float initialValue = 0.f;
		float lowerLimit = 0.f;
		float upperLimit = 0.f;
		float slope = 0.f;
		bool up = false;

	public:
		void Start(float value, bool newUp);
		float Current() const;
		void Up();
		void Down();

		void LowerLimit(float newLowerLimit)
		{
			lowerLimit = newLowerLimit;
		}
		float LowerLimit() const
		{
			return lowerLimit;
		}

		void UpperLimit(float newUpperLimit)
		{
			upperLimit = newUpperLimit;
		}
		float UpperLimit() const
		{
			return upperLimit;
		}

		void Slope(float newSlope);
		float Slope() const
		{
			return slope;
		}

		void UpDown(bool up)
		{
			if (up)
			{
				Up();
			}
			else
			{
				Down();
			}
		}
	};
}
