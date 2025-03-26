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

	void View::BeginSliderInternal(
		ComponentKey key,
		ExtendedSize2<Pos *> value,
		Pos2 minValue,
		Pos2 maxValue,
		Size thumbSize,
		ExtendedSize2<bool> maxInclusive
	)
	{
		auto &g = GetHost();
		BeginComponent(key);
		{
			auto &context = SetContext<SliderContext>(*GetCurrentComponent());
			if (context.background)
			{
				g.DrawStaticTexture(SliderGetBackgroundRect(), *context.background, 0xFFFFFFFF_argb);
			}
		}
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
		g.DrawRect(r, 0xFFFFFFFF_argb);
		auto &parentComponent = *GetParentComponent();
		SetPrimaryAxis(parentComponent.prevLayout.primaryAxis);
		SetPadding(thumbPadding);
		BeginComponent("inner");
		auto innerRect = GetRect();
		Pos2 draggerValue{ 0, 0 };
		auto outputSize = maxValue - minValue;
		Size2 clampToSize{ 0, 0 };
		{
			auto &context = GetContext<SliderContext>(*GetParentComponent());
			clampToSize = innerRect.size - (context.thumbRect.size - Size2{ 1, 1 });
		}
		auto inputSize = clampToSize - inclusiveOffset;
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			auto divBy = outputSize % psAxis;
			if (auto *valuePtr = value % psAxis; valuePtr && divBy)
			{
				draggerValue % psAxis = *valuePtr * (inputSize % psAxis) / divBy;
			}
		}
		ExtendedSize2<Pos *> draggerValuePtr{ &draggerValue.X, nullptr };
		if (value.Y)
		{
			draggerValuePtr.Y = &draggerValue.Y;
		}
		BeginDraggerInternal("thumb", draggerValuePtr, &mouseDown);
		SetSize(thumbSize);
		if (value.Y)
		{
			SetSizeSecondary(thumbSize);
		}
		auto thumbRect = GetRect();
		{
			auto &context = GetContext<SliderContext>(componentStore[(componentStack.end() - 3)->index]);
			context.thumbRect = thumbRect;
			context.rangeRect.pos = innerRect.pos + thumbRect.size / 2;
			context.rangeRect.size = clampToSize;
			context.minValue = minValue;
			context.maxValue = maxValue;
			context.inclusiveOffset = inclusiveOffset;
		}
		g.FillRect(thumbRect.Inset(-1), 0xFF000000_argb);
		g.DrawRect(thumbRect, 0xFFFFFFFF_argb);
		if (EndDragger())
		{
			for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
			{
				auto divBy = inputSize % psAxis;
				if (auto *valuePtr = value % psAxis; valuePtr && divBy)
				{
					*valuePtr = ((draggerValue % psAxis) * (outputSize % psAxis) + divBy - 1) / divBy;
				}
			}
		}
		EndComponent();
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
		auto &context = GetContext<SliderContext>(*GetCurrentComponent());
		context.changed = oldValue.X != *value.X;
		if (value.Y)
		{
			context.changed |= oldValue.Y != *value.Y;
		}
	}

	void View::BeginSlider(ComponentKey key, Pos &value, Pos minValue, Pos maxValue, Size thumbSize, bool maxInclusive)
	{
		BeginSliderInternal(key, { &value, nullptr }, { minValue, 0 }, { maxValue, 0 }, thumbSize, { maxInclusive, true });
	}

	void View::BeginSlider(ComponentKey key, Pos2 &value, Pos2 minValue, Pos2 maxValue, Size thumbSize, ExtendedSize2<bool> maxInclusive)
	{
		BeginSliderInternal(key, { &value.X, &value.Y }, minValue, maxValue, thumbSize, maxInclusive);
	}

	void View::SliderSetBackground(std::shared_ptr<StaticTexture> newBackground)
	{
		auto &context = GetContext<SliderContext>(*GetCurrentComponent());
		context.background = newBackground;
	}

	Rect View::SliderGetBackgroundRect() const
	{
		return GetRect().Inset(2);
	}

	bool View::EndSlider()
	{
		auto &context = GetContext<SliderContext>(*GetCurrentComponent());
		auto changed = context.changed;
		EndComponent();
		return changed;
	}

	bool View::Slider(ComponentKey key, Pos &value, Pos minValue, Pos maxValue)
	{
		BeginSlider(key, value, minValue, maxValue, GetHost().GetCommonMetrics().size, true);
		return EndSlider();
	}

	bool View::Slider(ComponentKey key, Pos2 &value, Pos2 minValue, Pos2 maxValue)
	{
		BeginSlider(key, value, minValue, maxValue, GetHost().GetCommonMetrics().size, { true, true });
		return EndSlider();
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
