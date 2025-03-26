#include "View.hpp"
#include "ViewUtil.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"

namespace Powder::Gui
{
	namespace
	{
		constexpr auto scrollbarTrackColor = 0x80FFFFFF_argb;
		constexpr auto scrollbarThumbColor = 0xFFFFFFFF_argb;
		constexpr auto wheelFactor         = 120.f;
		constexpr auto maxMomentum         = 420.f;
		constexpr auto wheelDirectFactor   = 20.f;
		constexpr View::Size maxThickness  = 6;
		constexpr View::Size minLength     = 17;
	}

	void View::BeginScrollpanel(ComponentKey key)
	{
		BeginScrollpanel(key, {});
	}

	void View::BeginScrollpanel(ComponentKey key, ScrollpanelDimension primary)
	{
		BeginScrollpanel(key, primary, { ScrollpanelDimension::Visibility::never });
	}

	void View::BeginScrollpanel(ComponentKey key, ScrollpanelDimension primary, ScrollpanelDimension secondary)
	{
		BeginPanel(key);
		SetHandleButtons(true);
		SetHandleWheel(true);
		auto &parentComponent = *GetParentComponent();
		auto &component = *GetCurrentComponent();
		auto &context = SetContext<ScrollpanelContext>(component);
		context.dimension = { primary, secondary };
		SetPrimaryAxis(parentComponent.prevLayout.primaryAxis);
	}

	void View::EndScrollpanel()
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<ScrollpanelContext>(component);
		context.overscroll = { 0, 0 };
		auto thickness = maxThickness;
		struct Side
		{
			bool show = false;
			bool reverseSide = false;
			Size pos = 0;
			Size size = 0;
		};
		auto r = GetRect();
		ExtendedSize2<Side> showSide;
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			auto xyAxis = AxisBase(component.prevLayout.primaryAxis) ^ psAxis;
			auto &dimension = context.dimension % psAxis;
			auto show = (dimension.visibility == ScrollpanelDimension::Visibility::always) ||
			            (dimension.visibility == ScrollpanelDimension::Visibility::ifUseful && (component.overflow % psAxis));
			auto &side = showSide % xyAxis;
			side.show = show;
			side.size = r.size % xyAxis;
		}
		{
			auto &primarySide   = showSide %  AxisBase(component.prevLayout.primaryAxis)     ;
			auto &secondarySide = showSide % (AxisBase(component.prevLayout.primaryAxis) ^ 1);
			if (primarySide.show)
			{
				secondarySide.size -= thickness;
				if (primarySide.reverseSide)
				{
					secondarySide.pos += thickness;
				}
			}
		}
		bool clearMouseDownPos = true;
		auto scrollDistance = GetScrollDistance();
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			auto xyAxis = AxisBase(component.prevLayout.primaryAxis) ^ psAxis;
			auto &side = showSide % xyAxis;
			if (side.show)
			{
				auto contentSpace = component.contentSpace      % psAxis;
				auto contentSize  = component.contentSize       % psAxis;
				auto overflow     = component.overflow          % psAxis;
				auto scroll       = component.prevLayout.scroll % psAxis;
				Rect trackRect{ r.TopLeft(), { thickness, thickness } };
				trackRect.pos  % xyAxis += side.pos;
				trackRect.size % xyAxis =  side.size;
				if (side.reverseSide)
				{
					component.layout.paddingBefore % (psAxis ^ 1) += thickness;
				}
				else
				{
					trackRect.pos % (xyAxis ^ 1) += r.size % (xyAxis ^ 1) - thickness;
					component.layout.paddingAfter % (psAxis ^ 1) += thickness;
				}
				auto &g = GetHost();
				g.FillRect(trackRect, scrollbarTrackColor);
				Rect thumbRect = trackRect;
				auto &thumbLength = thumbRect.size % xyAxis;
				auto &trackLength = trackRect.size % xyAxis;
				thumbLength = std::min(trackLength, contentSize ? std::max(minLength, contentSpace * trackLength / contentSize) : trackLength);
				thumbRect.pos % xyAxis = (trackRect.pos % xyAxis) + (overflow ? (scroll * (trackLength - thumbLength) / -overflow) : 0);
				g.FillRect(thumbRect, scrollbarThumbColor);
				auto &stickyScroll = context.stickyScroll % xyAxis;
				auto &overscroll = context.overscroll % xyAxis;
				if (IsMouseDown(SDL_BUTTON_LEFT))
				{
					auto m = *GetMousePos();
					clearMouseDownPos = false;
					if (context.mouseDownInThumbPos)
					{
						auto wantThumbPos = m - *context.mouseDownInThumbPos;
						stickyScroll.SetMomentum(0.f);
						auto freedom = (trackLength - thumbLength);
						stickyScroll.SetValue(freedom ? float(((wantThumbPos % xyAxis) - (trackRect.pos % xyAxis)) * -overflow / freedom) : 0.f);
					}
					else
					{
						if (thumbRect.Contains(m))
						{
							context.mouseDownInThumbPos = m - thumbRect.TopLeft();
						}
						else if (trackRect.Contains(m))
						{
							context.mouseDownInThumbPos = thumbRect.size / 2;
						}
					}
				}
				if (scrollDistance)
				{
					auto distance = *scrollDistance % xyAxis;
					if ((stickyScroll.GetValue() >= 0         && distance > 0) ||
					    (stickyScroll.GetValue() <= -overflow && distance < 0))
					{
						overscroll += distance;
					}
					if (g.GetMomentumScroll())
					{
						stickyScroll.SetMomentum(stickyScroll.GetMomentum() + distance * wheelFactor);
					}
					else
					{
						stickyScroll.SetValue(stickyScroll.GetValue() + distance * wheelDirectFactor);
					}
				}
				{
					auto momentum = stickyScroll.GetMomentum();
					if (std::abs(momentum) > maxMomentum)
					{
						stickyScroll.SetMomentum(std::copysign(maxMomentum, momentum));
					}
				}
				{
					auto value = stickyScroll.GetValue();
					auto valuePos = Pos(value);
					auto clampedValuePos = std::clamp(valuePos, -overflow, 0);
					if (valuePos != clampedValuePos)
					{
						stickyScroll.SetMomentum(0.f);
						stickyScroll.SetValue(float(clampedValuePos));
					}
				}
				component.layout.scroll % psAxis = Pos(stickyScroll.GetValue());
			}
		}
		if (clearMouseDownPos)
		{
			context.mouseDownInThumbPos.reset();
		}
		EndPanel();
	}

	View::Pos2 View::ScrollpanelGetOverscroll() const
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<ScrollpanelContext>(component);
		return context.overscroll;
	}

	void View::ScrollpanelSetScroll(View::Pos2 newScroll)
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<ScrollpanelContext>(component);
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			auto xyAxis = AxisBase(component.prevLayout.primaryAxis) ^ psAxis;
			auto &stickyScroll = context.stickyScroll % xyAxis;
			stickyScroll.SetMomentum(0);
			stickyScroll.SetValue(float(newScroll % xyAxis));
		}
	}
}
