#include "ModalWindow.h"

#include "SDLWindow.h"
#include "Appearance.h"
#include "Button.h"
#include "graphics/FontData.h"

#include <SDL.h>

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
		if (Quittable())
		{
			SDLWindow::Ref().PopBack(this);
		}
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
		auto toolTipToDraw = ToolTipToDraw();
		if (toolTipToDraw)
		{
			toolTipAnim.Up();
			toolTipBackup = toolTipToDraw;
		}
		else
		{
			toolTipAnim.Down();
		}
		auto *withFocus = ChildWithFocusDeep();
		if (withFocus && withFocus->WantTextInput())
		{
			SDLWindow::Ref().StartTextInput();
		}
		else
		{
			SDLWindow::Ref().StopTextInput();
		}
	}

	void ModalWindow::Cancel()
	{
		if (cancel)
		{
			cancel();
		}
		else
		{
			Quit();
		}
	}

	void ModalWindow::Okay()
	{
		if (okay)
		{
			okay();
		}
	}

	bool ModalWindow::MouseDown(Point current, int button)
	{
		if (!MouseForwardRect().Offset(AbsolutePosition()).Contains(current) && button == 1)
		{
			Cancel();
		}
		return false;
	}

	std::shared_ptr<const ToolTipInfo> ModalWindow::ToolTipToDraw() const
	{
		auto *child = ChildUnderMouseDeep();
		auto toolTip = child ? child->ToolTip() : nullptr;
		if (toolTip && toolTip->text.size())
		{
			return toolTip;
		}
		return nullptr;
	}

	void ModalWindow::DrawAfterChildren() const
	{
		if (drawToolTips)
		{
			auto &g = SDLWindow::Ref();
			auto &c = Appearance::colors.inactive.idle;
			auto toolTipToDraw = ToolTipToDraw();
			if (toolTipToDraw && toolTipToDraw->aboveSource)
			{
				auto *child = ChildUnderMouseDeep();
				auto cpos = child->AbsolutePosition();
				auto csize = child->Size();
				auto size = g.TextSize(toolTipToDraw->text) + Point{ 8, 5 };
				if (size.x < 20)
				{
					size.x = 20;
				}
				auto pos = Rect{ { 0, 0 }, Point{ WINDOWW, WINDOWH } - size }.Clamp({ cpos.x + (csize.x - size.x) / 2, cpos.y - size.y - 5 });
				g.DrawRect({ pos, size }, c.body);
				g.DrawRectOutline({ pos - Point{ 1, 1 }, size + Point{ 2, 2 } }, c.shadow);
				g.DrawRectOutline({ pos, size }, c.border);
				g.DrawText(pos + Point{ 4, 3 }, toolTipToDraw->text, c.text);
				auto tpos = cpos + Point{ csize.x / 2, 0 };
				for (auto i = 4; i >= 0; --i)
				{
					g.DrawLine(tpos + Point{ -i - 1, -i - 1 }, tpos + Point{ i + 1, -i - 1 }, c.shadow);
					g.DrawLine(tpos + Point{ -i    , -i - 2 }, tpos + Point{ i    , -i - 2 }, c.border);
					g.DrawLine(tpos + Point{ -i    , -i - 3 }, tpos + Point{ i    , -i - 3 }, c.body);
				}
			}
			else
			{
				if (!toolTipToDraw)
				{
					toolTipToDraw = toolTipBackup;
				}
				auto alpha = toolTipAnim.Current();
				if (alpha > 1.f) alpha = 1.f;
				auto alphaInt = int(alpha * 255);
				if (alphaInt && !toolTipToDraw->aboveSource)
				{
					uint32_t flags = 0U;
					if (shadedToolTips)
					{
						flags |= gui::SDLWindow::drawTextShaded;
					}
					g.DrawText(toolTipToDraw->pos, toolTipToDraw->text, { c.text.r, c.text.g, c.text.b, alphaInt }, flags);
				}
			}
		}
		Component::DrawAfterChildren();
	}
}
