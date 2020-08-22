#pragma once

#include "Button.h"

#include <functional>

namespace gui
{
	class CheckBox : public Static
	{
	public:
		using ChangeCallback = std::function<void (bool value)>;

	private:
		ChangeCallback change;
		bool value = false;
		void OptionToText();

	public:
		void Draw() const final override;
		void MouseClick(Point current, int button, int clicks) final override;

		void Change(ChangeCallback newChange)
		{
			change = newChange;
		}
		ChangeCallback Change() const
		{
			return change;
		}

		void Value(bool newValue)
		{
			value = newValue;
		}
		bool Value() const
		{
			return value;
		}

		void Size(Point newSize) final override;
		Point Size() const
		{
			return Static::Size();
		}
	};
}
