#pragma once

#include "Component.h"
#include "LinearDelayAnimation.h"

#include <cstdint>

namespace gui
{
	class Button;
	class SDLWindow;
	
	class ModalWindow : public Component
	{
		bool drawToolTips = true;
		bool pushed = false;
		int fpsLimit = 60;
		int rpsLimit = 0;
		LinearDelayAnimation toolTipAnim;
		ToolTipInfo toolTipBackup;

		std::shared_ptr<Button> cancelButton;
		std::shared_ptr<Button> okayButton;

		const ToolTipInfo *ToolTipToDraw() const;

	public:
		ModalWindow();

		virtual void Tick() override;
		virtual void Quit() override;
		virtual void Draw() const override;

		int FPSLimit() const
		{
			return fpsLimit;
		}

		int RPSLimit() const
		{
			return rpsLimit;
		}

		virtual bool WantsBackdrop() const;

		virtual bool MouseDown(Point current, int button);
		virtual bool KeyPress(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt);
		virtual void Cancel();
		virtual void Okay();

		friend class SDLWindow;

		void CancelButton(std::shared_ptr<Button> newCancelButton)
		{
			cancelButton = newCancelButton;
		}
		std::shared_ptr<Button> CancelButton() const
		{
			return cancelButton;
		}

		void OkayButton(std::shared_ptr<Button> newOkayButton)
		{
			okayButton = newOkayButton;
		}
		std::shared_ptr<Button> OkayButton() const
		{
			return okayButton;
		}

		void DrawToolTips(bool newDrawToolTips)
		{
			drawToolTips = newDrawToolTips;
		}
		bool DrawToolTips() const
		{
			return drawToolTips;
		}
	};
}
