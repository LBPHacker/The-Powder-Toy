#include "ScrollPanel.h"

#include "SDLWindow.h"

namespace gui
{
	constexpr auto trackDepth = 6;
	constexpr auto draggerMinSize = 20;
	constexpr auto scrollBarPopupTime = 0.1f;
	constexpr int Point::*pointXY[2] = { &Point::x, &Point::y };
	constexpr auto momentumMin = 30.f;
	constexpr auto momentumMax = 420.f;
	constexpr auto momentumHalflife = 0.6f;
	constexpr auto momentumStep = 120.f;
	constexpr auto plainStep = 20;

	ScrollPanel::ScrollPanel()
	{
		scrollBarPopup.LowerLimit(0);
		scrollBarPopup.UpperLimit(1);
		scrollBarPopup.Slope(1 / scrollBarPopupTime);
		for (auto dim = 0; dim < 2; ++dim)
		{
			scroll[dim].anim.SmallThreshold(momentumMin);
			scroll[dim].anim.HalfLife(momentumHalflife);
			scroll[dim].anim.Limit(momentumMax);
		}
		ClipChildren(true);
		interior = EmplaceChild<Component>();
	}

	void ScrollPanel::Tick()
	{
		scrollBarPopup.UpDown(UnderMouse());
		if (!interior)
		{
			return;
		}
		auto size = Size();
		auto offset = interior->Position();
		auto isize = interior->Size();
		auto sizeDiff = size - isize;
		bool offsetChanged = false;
		bool enableChanged = false;
		for (auto dim = 0; dim < 2; ++dim)
		{
			auto xy = pointXY[dim];
			auto oldEnable = scroll[dim].enabled;
			scroll[dim].enabled = sizeDiff.*xy < 0;
			if (scroll[dim].enabled != oldEnable)
			{
				enableChanged = true;
			}
			scroll[dim].draggerSize = 0;
			scroll[dim].draggerPosition = 0;
			scroll[dim].anim.CancelIfSmall();
			if (!scroll[dim].anim.Small())
			{
				offset.*xy = scroll[dim].anim.EffectivePosition();
				offsetChanged = true;
			}
			auto lowTarget = sizeDiff.*xy;
			if (!scroll[dim].alignHigh)
			{
				lowTarget = std::min(lowTarget, 0);
			}
			if (offset.*xy < lowTarget)
			{
				offset.*xy = lowTarget;
				scroll[dim].anim.Cancel(offset.*xy);
				offsetChanged = true;
			}
			auto highTarget = 0;
			if (scroll[dim].alignHigh)
			{
				highTarget = std::max(highTarget, sizeDiff.*xy);
			}
			if (offset.*xy > highTarget)
			{
				offset.*xy = highTarget;
				scroll[dim].anim.Cancel(offset.*xy);
				offsetChanged = true;
			}
			if (scroll[dim].enabled && isize.*xy != 0)
			{
				scroll[dim].draggerSize = size.*xy * size.*xy / isize.*xy;
				if (scroll[dim].draggerSize < draggerMinSize)
				{
					scroll[dim].draggerSize = draggerMinSize;
				}
				scroll[dim].draggerPosition = offset.*xy * (size.*xy - scroll[dim].draggerSize) / sizeDiff.*xy;
			}
		}
		if (enableChanged)
		{
			Size(Size()); // * Trigger own Size method which calls MouseForwardRect.
		}
		if (offsetChanged)
		{
			interior->Position(offset);
		}
	}

	void ScrollPanel::DrawAfterChildren() const
	{
		auto effectiveTrackDepth = int(scrollBarPopup.Current() * trackDepth);
		auto abs = AbsolutePosition();
		auto size = Size();
		auto &g = SDLWindow::Ref();
		auto cTrack   = Color{ 125, 125, 125, 100 };
		auto cDragger = Color{ 255, 255, 255, 255 };
		auto shaveOffX = (scroll[0].enabled && scroll[1].enabled) ? effectiveTrackDepth : 0;
		if (scrollBarVisible && scroll[0].enabled)
		{
			g.DrawRect(Rect{ abs + Point{ 0, size.y - effectiveTrackDepth }, Point{ size.x - shaveOffX, effectiveTrackDepth } }, cTrack );
			g.DrawRect(Rect{ abs + Point{ scroll[0].draggerPosition, size.y - effectiveTrackDepth }, Point{ scroll[0].draggerSize, effectiveTrackDepth } }, cDragger );
		}
		if (scrollBarVisible && scroll[1].enabled)
		{
			g.DrawRect(Rect{ abs + Point{ size.x - effectiveTrackDepth, 0 }, Point{ effectiveTrackDepth,             size.y } }, cTrack );
			g.DrawRect(Rect{ abs + Point{ size.x - effectiveTrackDepth, scroll[1].draggerPosition }, Point{ effectiveTrackDepth, scroll[1].draggerSize } }, cDragger );
		}
	}

	bool ScrollPanel::MouseWheel(Point, int distance, int direction)
	{
		if (!interior)
		{
			return false;
		}
		auto size = Size();
		auto offset = interior->Position();
		auto isize = interior->Size();
		auto sizeDiff = size - isize;
		if (direction == 1 && SDLWindow::Ref().ModShift())
		{
			direction = 0;
		}
		if (direction == 0 || direction == 1)
		{
			auto xy = pointXY[direction];
			if ((offset.*xy > sizeDiff.*xy && distance < 0) || (offset.*xy < 0 && distance > 0))
			{
				if (SDLWindow::Ref().MomentumScroll())
				{
					scroll[direction].anim.Accelerate(distance * momentumStep);
				}
				else
				{
					offset.*xy += distance * plainStep;
					interior->Position(offset);
				}
				return true;
			}
		}
		return false;
	}

	void ScrollPanel::MouseDragMove(Point current, int button)
	{
		if (scrollBarVisible && button == SDL_BUTTON_LEFT)
		{
			auto offset = interior->Position();
			auto local = current - AbsolutePosition();
			for (auto dim = 0; dim < 2; ++dim)
			{
				if (scroll[dim].dragActive)
				{
					auto xy = pointXY[dim];
					auto size = Size();
					auto isize = interior->Size();
					auto sizeDiff = size - isize;
					scroll[dim].draggerPosition = local.*xy - scroll[dim].dragInitialMouse + scroll[dim].dragInitialDragger;
					offset.*xy = scroll[dim].draggerPosition * sizeDiff.*xy / (size.*xy - scroll[dim].draggerSize);
					scroll[dim].anim.Cancel(offset.*xy);
				}
			}
			interior->Position(offset);
		}
	}

	void ScrollPanel::MouseDragBegin(Point current, int button)
	{
		if (scrollBarVisible && button == SDL_BUTTON_LEFT)
		{
			auto offset = interior->Position();
			auto local = current - AbsolutePosition();
			for (auto rev = 0; rev < 2; ++rev)
			{
				auto yx = pointXY[rev];
				auto dim = 1 - rev;
				auto xy = pointXY[dim];
				if (scroll[dim].enabled && local.*yx > Size().*yx - trackDepth)
				{
					scroll[dim].dragActive = true;
					if (scroll[dim].draggerPosition + scroll[dim].draggerSize <= local.*xy || scroll[dim].draggerPosition > local.*xy)
					{
						scroll[dim].draggerPosition = local.*xy - scroll[dim].draggerSize / 2;
					}
					auto size = Size();
					auto isize = interior->Size();
					auto sizeDiff = size - isize;
					scroll[dim].dragInitialDragger = scroll[dim].draggerPosition;
					scroll[dim].dragInitialMouse = local.*xy;
					offset.*xy = scroll[dim].draggerPosition * sizeDiff.*xy / (size.*xy - scroll[dim].draggerSize);
					scroll[dim].anim.Cancel(offset.*xy);
					CaptureMouse(true);
					break; // * Note how the loop header and this break statement prioritises vertical scrolling over horizontal.
				}
			}
			interior->Position(offset);
		}
	}

	void ScrollPanel::MouseDragEnd(Point current, int button)
	{
		if (scrollBarVisible && button == SDL_BUTTON_LEFT)
		{
			scroll[0].dragActive = false;
			scroll[1].dragActive = false;
			CaptureMouse(false);
		}
	}

	void ScrollPanel::ScrollBarVisible(bool newScrollBarVisible)
	{
		scrollBarVisible = newScrollBarVisible;
		Size(Size()); // * Trigger own Size method which calls MouseForwardRect.
	}

	void ScrollPanel::Size(Point newSize)
	{
		Component::Size(newSize);
		MouseForwardRect({ Point{ 0, 0 }, newSize - Point{ scrollBarVisible && scroll[0].enabled ? trackDepth : 0, scrollBarVisible && scroll[1].enabled ? trackDepth : 0 } });
	}

	void ScrollPanel::Offset(Point offset)
	{
		interior->Position(offset);
		for (auto dim = 0; dim < 2; ++dim)
		{
			scroll[dim].anim.Cancel(offset.*pointXY[dim]);
		}
	}
}
