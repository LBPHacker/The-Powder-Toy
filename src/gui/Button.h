#pragma once

#include "Static.h"

#include <functional>

namespace gui
{
	class Button : public Static
	{
	public:
		using ClickCallback = std::function<void ()>;
		enum ActiveTextMode
		{
			activeTextInverted,
			activeTextDarkened,
		};

	private:
		bool stuck = false;
		bool drawBody = true;
		ClickCallback click;
		Color bodyColor;
		ActiveTextMode activeTextMode = activeTextInverted;

		void SetToolTip();
		void ResetToolTip();
		void UpdateToolTip();

	public:
		Button();

		void Draw() const override;
		bool MouseDown(Point current, int button) final override;
		void MouseClick(Point current, int button, int clicks) override;
		bool KeyPress(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt) final override;
		bool KeyRelease(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt) final override;

		void Click(ClickCallback newClick)
		{
			click = newClick;
		}
		ClickCallback Click() const
		{
			return click;
		}

		void Stuck(bool newStuck)
		{
			stuck = newStuck;
		}
		bool Stuck() const
		{
			return stuck;
		}

		void BodyColor(Color newBodyColor)
		{
			bodyColor = newBodyColor;
		}
		Color BodyColor() const
		{
			return bodyColor;
		}

		void ActiveText(ActiveTextMode newActiveTextMode)
		{
			activeTextMode = newActiveTextMode;
		}
		ActiveTextMode ActiveText() const
		{
			return activeTextMode;
		}

		void DrawBody(bool newDrawBody)
		{
			drawBody = newDrawBody;
		}
		bool DrawBody() const
		{
			return drawBody;
		}

		virtual void HandleClick(int button);
		void TriggerClick();
	};
}
