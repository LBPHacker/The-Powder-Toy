#include "View.hpp"
#include "ViewUtil.hpp"
#include "Common/Div.hpp"
#include "Gui/Host.hpp"
#include "Gui/SdlAssert.hpp"

namespace Powder::Gui
{
	namespace
	{
		constexpr View::Pos maxExtremeAbs       = 1'000'000'000;
		constexpr Gui::View::Size thumbPadding  =             2;
		constexpr Gui::View::Size stripeSpacing =             2;
	}

	bool View::EndSliderInternal(
		ExtendedSize2<Pos *> value,
		Pos2 minValue,
		Pos2 maxValue,
		Size thumbSize,
		ExtendedSize2<bool> maxInclusive
	)
	{
		SetHandleButtons(true);
		auto mouseDown = IsMouseDown(SDL_BUTTON_LEFT);
		Pos2 inclusiveOffset{ 0, 0 };
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			inclusiveOffset % psAxis = (maxInclusive % psAxis) ? 1 : 0;
			minValue % psAxis = std::clamp(minValue % psAxis, -maxExtremeAbs, maxExtremeAbs);
			maxValue % psAxis = std::clamp(maxValue % psAxis, minValue % psAxis + inclusiveOffset % psAxis, maxExtremeAbs + 1);
		}
		auto oldValue = Pos2{ *value.X, 0 };
		if (value.Y)
		{
			oldValue.Y = *value.Y;
		}
		auto r = GetRect();
		auto &g = GetHost();
		g.DrawRect(r, 0xFFFFFFFF_argb);
		auto &parentComponent = *GetParentComponent();
		SetPrimaryAxis(parentComponent.prevLayout.primaryAxis);
		SetPadding(thumbPadding);
		BeginComponent("inner");
		auto innerRect = GetRect();
		BeginComponent("thumb");
		auto &context = GetContext<SliderContext>(componentStore[(componentStack.end() - 3)->index]);
		auto thumbRect = GetRect();
		SetSize(thumbSize);
		if (value.Y)
		{
			SetSizeSecondary(thumbSize);
		}
		auto clampToSize = innerRect.size - (thumbRect.size - Size2{ 1, 1 });
		context.rangeRect.pos = innerRect.pos + thumbRect.size / 2;
		context.rangeRect.size = clampToSize;
		Pos2 thumbPos{ 0, 0 };
		bool possiblyChanged = false;
		context.minValue = minValue;
		context.maxValue = maxValue;
		context.inclusiveOffset = inclusiveOffset;
		auto outputSize = maxValue - minValue;
		auto inputSize = clampToSize - inclusiveOffset;
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			auto divBy = outputSize % psAxis;
			if (auto *valuePtr = value % psAxis; valuePtr && divBy)
			{
				thumbPos % psAxis = *valuePtr * (inputSize % psAxis) / divBy;
			}
			if (mouseDown && context.thumbPosAtMouseDown)
			{
				auto m = *GetMousePos();
				thumbPos % psAxis = *context.thumbPosAtMouseDown % psAxis + m % psAxis - *context.mouseDownPos % psAxis;
				possiblyChanged = true;
			}
		}
		thumbPos = RobustClamp(thumbPos, clampToSize.OriginRect());
		if (mouseDown)
		{
			if (!context.mouseDownPos)
			{
				context.mouseDownPos = *GetMousePos();
				if (IsHovered())
				{
					context.thumbPosAtMouseDown = thumbPos;
				}
				else
				{
					context.thumbPosAtMouseDown = (*context.mouseDownPos - thumbRect.size / 2) - innerRect.pos;
				}
			}
		}
		else
		{
			context.mouseDownPos.reset();
			context.thumbPosAtMouseDown.reset();
		}
		if (possiblyChanged)
		{
			for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
			{
				auto divBy = inputSize % psAxis;
				if (auto *valuePtr = value % psAxis; valuePtr && divBy)
				{
					*valuePtr = ((thumbPos % psAxis) * (outputSize % psAxis) + divBy - 1) / divBy;
				}
			}
		}
		ForcePosition(innerRect.pos + thumbPos);
		EndComponent();
		EndComponent();
		g.FillRect(thumbRect.Inset(-1), 0xFF000000_argb);
		g.DrawRect(thumbRect, 0xFFFFFFFF_argb);
		{
			auto stripeOffset = thumbRect.pos + thumbRect.size / 2;
			if (value.Y)
			{
				for (auto p : RectBetween(Pos2{ -1, -1 }, Pos2{ 1, 1 }))
				{
					g.DrawPoint(p * stripeSpacing + stripeOffset, 0xFF808080_argb);
				}
			}
			else
			{
				for (int32_t p = -1; p <= 1; ++p)
				{
					auto pos1 = Pos2{ p, -1 } * stripeSpacing;
					auto pos2 = pos1 + Pos2{ 0, stripeSpacing * 2 };
					if (parentComponent.prevLayout.primaryAxis == Axis::horizontal)
					{
						std::swap(pos1.X, pos1.Y);
						std::swap(pos2.X, pos2.Y);
					}
					g.DrawLine(pos1 + stripeOffset, pos2 + stripeOffset, 0xFF808080_argb);
				}
			}
		}
		if (value.Y)
		{
			SetCursor(Cursor::sizeall);
		}
		else
		{
			if (parentComponent.prevLayout.primaryAxis == Axis::horizontal)
			{
				SetCursor(Cursor::sizens);
			}
			else
			{
				SetCursor(Cursor::sizewe);
			}
		}
		EndComponent();
		bool changed = oldValue.X != *value.X;
		if (value.Y)
		{
			changed |= oldValue.Y != *value.Y;
		}
		return changed;
	}

	void View::BeginSlider(ComponentKey key)
	{
		BeginComponent(key);
		SetContext<SliderContext>(*GetCurrentComponent());
	}

	bool View::EndSlider(Pos &value, Pos minValue, Pos maxValue, Size thumbSize, bool maxInclusive)
	{
		return EndSliderInternal({ &value, nullptr }, { minValue, 0 }, { maxValue, 0 }, thumbSize, { maxInclusive, true });
	}

	bool View::EndSlider(Pos2 &value, Pos2 minValue, Pos2 maxValue, Size thumbSize, ExtendedSize2<bool> maxInclusive)
	{
		return EndSliderInternal({ &value.X, &value.Y }, minValue, maxValue, thumbSize, maxInclusive);
	}

	bool View::Slider(ComponentKey key, Pos &value, Pos minValue, Pos maxValue)
	{
		BeginSlider(key);
		return EndSlider(value, minValue, maxValue, GetHost().GetCommonMetrics().size, true);
	}

	bool View::Slider(ComponentKey key, Pos2 &value, Pos2 minValue, Pos2 maxValue)
	{
		BeginSlider(key);
		return EndSlider(value, minValue, maxValue, GetHost().GetCommonMetrics().size, { true, true });
	}

	Rect View::SliderGetRange() const
	{
		auto &context = GetContext<SliderContext>(*GetCurrentComponent());
		return context.rangeRect;
	}

	View::Pos View::SliderMapPosition(Pos pos) const
	{
		return SliderMapPosition({ pos, 0 }).X;
	}

	View::Pos2 View::SliderMapPosition(Pos2 pos) const
	{
		auto range = SliderGetRange();
		auto &context = GetContext<SliderContext>(*GetCurrentComponent());
		pos -= range.pos;
		auto outputSize = context.maxValue - context.minValue;
		auto inputSize = range.size - context.inclusiveOffset;
		Pos2 value{ 0, 0 };
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			auto divBy = inputSize % psAxis;
			if (divBy)
			{
				value % psAxis = ((pos % psAxis) * (outputSize % psAxis) + divBy - 1) / divBy;
			}
		}
		return value;
	}
}
