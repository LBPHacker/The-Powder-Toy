#pragma once

#include "Button.h"

#include <functional>

namespace gui
{
	class DropDown : public Button
	{
	public:
		using ChangeCallback = std::function<void (int)>;

	private:
		ChangeCallback change;
		int value = 0;
		std::vector<String> options;
		void OptionToText();

		using Button::Click;

	public:
		DropDown();

		void Change(ChangeCallback newChange)
		{
			change = newChange;
		}
		ChangeCallback Change() const
		{
			return change;
		}

		void Value(int newValue);
		int Value() const
		{
			return value;
		}

		void Options(const std::vector<String> &newOptions);
		const std::vector<String> &Options() const
		{
			return options;
		}
	};
}
