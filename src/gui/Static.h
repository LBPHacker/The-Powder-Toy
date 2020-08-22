#pragma once

#include "Color.h"
#include "Component.h"
#include "Alignment.h"
#include "common/String.h"

namespace gui
{
	class Static : public Component
	{
	protected:
		String text;
		int align = Alignment::horizLeft | Alignment::vertCenter;
		Point drawTextAt = Point{ 0, 0 };
		Rect textRect = Rect{ Point{ 0, 0 }, Point{ 0, 0 } };
		Color textColor;
		virtual void UpdateText();

	public:
		Static();
		
		virtual void Draw() const;

		void Text(const String &newText);
		const String &Text() const
		{
			return text;
		}

		void Align(int newAlign);
		const int Align() const
		{
			return align;
		}

		virtual void Size(Point newSize) override;
		Point Size() const
		{
			return Component::Size();
		}

		void TextRect(Rect newTextRect);
		Rect TextRect() const
		{
			return textRect;
		}

		void TextColor(Color newTextColor)
		{
			textColor = newTextColor;
		}
		Color TextColor() const
		{
			return textColor;
		}
	};
}
