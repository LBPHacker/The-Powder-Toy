#include "View.hpp"
#include "ViewUtil.hpp"
#include "Gui/SdlAssert.hpp"

namespace Powder::Gui
{
	void View::BeginDraggerInternal(
		ComponentKey key,
		ExtendedSize2<Pos *> value,
		bool *mouseDown
	)
	{
		auto innerRect = GetRect();
		BeginComponent(key);
		auto &context = SetContext<DraggerContext>(*GetCurrentComponent());
		auto thumbRect = GetRect();
		auto clampToSize = innerRect.size - (thumbRect.size - Size2{ 1, 1 });
		Pos2 thumbPos{ 0, 0 };
		context.changed = false;
		bool effectiveMouseDown = false;
		if (mouseDown)
		{
			effectiveMouseDown = *mouseDown;
		}
		else
		{
			SetHandleButtons(true);
			effectiveMouseDown = IsMouseDown(SDL_BUTTON_LEFT);
			if (value.Y)
			{
				SetCursor(Cursor::sizeall);
			}
			else
			{
				if (GetParentComponent()->prevLayout.primaryAxis == Axis::horizontal)
				{
					SetCursor(Cursor::sizewe);
				}
				else
				{
					std::swap(value.Y, value.X);
					SetCursor(Cursor::sizens);
				}
			}
		}
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			if (auto *valuePtr = value % psAxis)
			{
				thumbPos % psAxis = *valuePtr;
			}
			if (effectiveMouseDown && context.posAtMouseDown)
			{
				auto m = *GetMousePos();
				thumbPos % psAxis = *context.posAtMouseDown % psAxis + m % psAxis - *context.mouseDownPos % psAxis;
				context.changed = true;
			}
		}
		thumbPos = RobustClamp(thumbPos, clampToSize.OriginRect());
		if (effectiveMouseDown)
		{
			if (!context.mouseDownPos)
			{
				context.mouseDownPos = *GetMousePos();
				if (IsHovered())
				{
					context.posAtMouseDown = thumbPos;
				}
				else
				{
					context.posAtMouseDown = (*context.mouseDownPos - thumbRect.size / 2) - innerRect.pos;
				}
			}
		}
		else
		{
			context.mouseDownPos.reset();
			context.posAtMouseDown.reset();
		}
		ForcePosition(innerRect.pos + thumbPos);
		if (context.changed)
		{
			for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
			{
				if (auto *valuePtr = value % psAxis)
				{
					*valuePtr = thumbPos % psAxis;
				}
			}
		}
	}

	void View::BeginDragger(ComponentKey key, Pos &value)
	{
		BeginDraggerInternal(key, { &value, nullptr }, nullptr);
	}

	void View::BeginDragger(ComponentKey key, Pos2 &value)
	{
		BeginDraggerInternal(key, { &value.X, &value.Y }, nullptr);
	}

	bool View::EndDragger()
	{
		auto &context = GetContext<DraggerContext>(*GetCurrentComponent());
		auto changed = context.changed;
		EndComponent();
		return changed;
	}
}
