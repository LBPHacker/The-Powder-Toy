#include "ModalWindow.h"

#include "SDLWindow.h"
#include "Appearance.h"
#include "Button.h"

namespace gui
{
	ModalWindow::ModalWindow()
	{
		toolTipAnim.LowerLimit(0);
		toolTipAnim.UpperLimit(2);
		toolTipAnim.Slope(1);
	}

	void ModalWindow::Quit()
	{
		SDLWindow::Ref().PopBack(this);
	}

	bool ModalWindow::WantsBackdrop() const
	{
		return false;
	}

	bool ModalWindow::KeyPress(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt)
	{
		switch (sym)
		{
		case SDLK_ESCAPE:
			Cancel();
			break;

		case SDLK_KP_ENTER:
		case SDLK_RETURN:
			Okay();
			break;

		default:
			break;
		}
		return false;
	}

	void ModalWindow::Tick()
	{
		auto *toolTipToDraw = ToolTipToDraw();
		if (toolTipToDraw)
		{
			toolTipAnim.Up();
			toolTipBackup = *toolTipToDraw;
		}
		else
		{
			toolTipAnim.Down();
		}
	}

	void ModalWindow::Cancel()
	{
		if (cancelButton)
		{
			cancelButton->TriggerClick();
		}
		else
		{
			Quit();
		}
	}

	void ModalWindow::Okay()
	{
		if (okayButton)
		{
			okayButton->TriggerClick();
		}
		else
		{
			Quit();
		}
	}

	bool ModalWindow::MouseDown(Point current, int button)
	{
		if (!Rect{ AbsolutePosition(), Size() }.Contains(current) && button == 1)
		{
			Cancel();
		}
		return false;
	}

	const ToolTipInfo *ModalWindow::ToolTipToDraw() const
	{
		auto *child = ChildUnderMouseDeep();
		auto *toolTipToDraw = child && child->ToolTip().text.size() ? &child->ToolTip() : nullptr;
		return toolTipToDraw;
	}

	void ModalWindow::Draw() const
	{
		if (drawToolTips)
		{
			auto alpha = toolTipAnim.Current();
			if (alpha > 1.f) alpha = 1.f;
			auto alphaInt = int(alpha * 255);
			if (alphaInt)
			{
				auto *toolTipToDraw = ToolTipToDraw();
				toolTipToDraw = toolTipToDraw ? toolTipToDraw : &toolTipBackup;
				SDLWindow::Ref().DrawText(toolTipToDraw->pos, toolTipToDraw->text, gui::Color{ 0xFF, 0xFF, 0xFF, alphaInt });
			}
		}
	}
}
