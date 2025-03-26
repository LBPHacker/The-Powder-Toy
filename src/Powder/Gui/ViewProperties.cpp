#include "View.hpp"
#include "ViewUtil.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"

namespace Powder::Gui
{
	void View::SetPrimaryAxis(Axis newPrimaryAxis)
	{
		GetCurrentComponent()->layout.primaryAxis = newPrimaryAxis;
	}

	void View::SetLayered(bool newLayered)
	{
		GetCurrentComponent()->layout.layered = newLayered;
	}

	void View::SetImmaterial(bool newImmaterial)
	{
		GetCurrentComponent()->layout.immaterial = newImmaterial;
	}

	void View::SetMinSize(MinSize newMinSize)
	{
		ClampSize(newMinSize);
		GetCurrentComponent()->layout.minSize.X = newMinSize;
	}

	void View::SetMinSizeSecondary(MinSize newMinSizeSecondary)
	{
		ClampSize(newMinSizeSecondary);
		GetCurrentComponent()->layout.minSize.Y = newMinSizeSecondary;
	}

	void View::SetMaxSize(MaxSize newMaxSize)
	{
		ClampSize(newMaxSize);
		GetCurrentComponent()->layout.maxSize.X = newMaxSize;
	}

	void View::SetMaxSizeSecondary(MaxSize newMaxSizeSecondary)
	{
		ClampSize(newMaxSizeSecondary);
		GetCurrentComponent()->layout.maxSize.Y = newMaxSizeSecondary;
	}

	void View::SetSize(SetSizeSize newSize)
	{
		ClampSize(newSize);
		if (auto *size = std::get_if<Size>(&newSize))
		{
			SetMinSize(*size);
			SetMaxSize(*size);
		}
		else if (std::holds_alternative<SpanAll>(newSize))
		{
			SetMinSize(MinSizeFitChildren{});
			SetMaxSize(MaxSizeInfinite{});
		}
		else
		{
			auto size = host.GetCommonMetrics().size;
			SetMinSize(size);
			SetMaxSize(size);
		}
	}

	void View::SetSizeSecondary(SetSizeSize newSizeSecondary)
	{
		ClampSize(newSizeSecondary);
		if (auto *size = std::get_if<Size>(&newSizeSecondary))
		{
			SetMinSizeSecondary(*size);
			SetMaxSizeSecondary(*size);
		}
		else if (std::holds_alternative<SpanAll>(newSizeSecondary))
		{
			SetMinSizeSecondary(MinSizeFitChildren{});
			SetMaxSizeSecondary(MaxSizeInfinite{});
		}
		else
		{
			auto sizeSecondary = host.GetCommonMetrics().size;
			SetMinSizeSecondary(sizeSecondary);
			SetMaxSizeSecondary(sizeSecondary);
		}
	}

	void View::SetScroll(Size newScroll)
	{
		ClampPos(newScroll);
		GetCurrentComponent()->layout.scroll.X = newScroll;
	}

	void View::SetScrollSecondary(Size newScrollSecondary)
	{
		ClampPos(newScrollSecondary);
		GetCurrentComponent()->layout.scroll.Y = newScrollSecondary;
	}

	void View::ForcePosition(std::optional<Pos2> newPosition)
	{
		GetCurrentComponent()->layout.forcePosition = newPosition;
	}

	void View::SetAlignment(Alignment newAlignment)
	{
		GetCurrentComponent()->layout.alignment.X = newAlignment;
	}

	void View::SetAlignmentSecondary(Alignment newAlignmentSecondary)
	{
		GetCurrentComponent()->layout.alignment.Y = newAlignmentSecondary;
	}

	void View::SetOrder(Order newOrder)
	{
		GetCurrentComponent()->layout.order.X = newOrder;
	}

	void View::SetOrderSecondary(Order newOrderSecondary)
	{
		GetCurrentComponent()->layout.order.Y = newOrderSecondary;
	}

	void View::SetSpacing(SetSpacingSize newSpacing)
	{
		auto size = host.GetCommonMetrics().spacing;
		if (auto *sizeValue = std::get_if<Size>(&newSpacing))
		{
			size = *sizeValue;
		}
		ClampPos(size); // spacing can be negative
		GetCurrentComponent()->spacing = size;
	}

	void View::AddSpacing(Size addSpacing)
	{
		extraSpacing += addSpacing;
		ClampPos(extraSpacing);
	}

	void View::SetPadding(SetPaddingSize newPadding)
	{
		auto size = host.GetCommonMetrics().padding;
		if (auto *sizeValue = std::get_if<Size>(&newPadding))
		{
			size = *sizeValue;
		}
		SetPadding(size, size, size, size);
	}

	void View::SetPadding(Size newPadding, Size newPaddingSecondary)
	{
		SetPadding(newPadding, newPadding, newPaddingSecondary, newPaddingSecondary);
	}

	void View::SetPadding(Size newPaddingBefore, Size newPaddingAfter, Size newPaddingSecondary)
	{
		SetPadding(newPaddingBefore, newPaddingAfter, newPaddingSecondary, newPaddingSecondary);
	}

	void View::SetPadding(Size newPaddingBefore, Size newPaddingAfter, Size newPaddingSecondaryBefore, Size newPaddingSecondaryAfter)
	{
		auto &layout = GetCurrentComponent()->layout;
		layout.paddingBefore.X = newPaddingBefore;
		layout.paddingAfter .X = newPaddingAfter;
		layout.paddingBefore.Y = newPaddingSecondaryBefore;
		layout.paddingAfter .Y = newPaddingSecondaryAfter;
		ClampSize(layout.paddingBefore);
		ClampSize(layout.paddingAfter);
	}

	void View::SetTextFirstLineIndent(Size newFirstLineIndent)
	{
		auto &content = GetCurrentComponent()->content;
		content.textFirstLineIndent = newFirstLineIndent;
		ClampSize(content.textFirstLineIndent);
	}

	void View::SetTextPadding(Size newPadding)
	{
		SetTextPadding(newPadding, newPadding, newPadding, newPadding);
	}

	void View::SetTextPadding(Size newHorizontalPadding, Size newVerticalPadding)
	{
		SetTextPadding(newHorizontalPadding, newHorizontalPadding, newVerticalPadding, newVerticalPadding);
	}

	void View::SetTextPadding(Size newHorizontalPaddingBefore, Size newHorizontalPaddingAfter, Size newVerticalPaddingBefore, Size newVerticalPaddingAfter)
	{
		auto &content = GetCurrentComponent()->content;
		content.textPaddingBefore.X = newHorizontalPaddingBefore;
		content.textPaddingAfter .X = newHorizontalPaddingAfter;
		content.textPaddingBefore.Y = newVerticalPaddingBefore;
		content.textPaddingAfter .Y = newVerticalPaddingAfter;
		ClampSize(content.textPaddingBefore);
		ClampSize(content.textPaddingAfter);
	}

	void View::SetEnabled(bool newEnabled)
	{
		auto &content = GetCurrentComponent()->content;
		content.enabled = newEnabled;
	}

	void View::SetHandleMouse(bool newHandleMouse)
	{
		GetCurrentComponent()->handleMouse = newHandleMouse;
	}

	void View::SetHandleButtons(bool newHandleButtons)
	{
		GetCurrentComponent()->handleButtons = newHandleButtons;
	}

	void View::SetHandleWheel(bool newHandleWheel)
	{
		GetCurrentComponent()->handleWheel = newHandleWheel;
	}

	void View::SetHandleInput(bool newHandleInput)
	{
		GetCurrentComponent()->handleInput = newHandleInput;
	}

#if DebugGuiView
	void View::SetDebugMe(bool newDebugMe)
	{
		GetCurrentComponent()->debugMe = newDebugMe;
	}
#endif

	void View::SetTextAlignment(Alignment newHorizontalTextAlignment, Alignment newVerticalTextAlignment)
	{
		GetCurrentComponent()->content.horizontalTextAlignment = newHorizontalTextAlignment;
		GetCurrentComponent()->content.verticalTextAlignment = newVerticalTextAlignment;
	}

	void View::SetRootRect(std::optional<Rect> newRootRect)
	{
		if (newRootRect)
		{
			ClampPos(newRootRect->pos);
			ClampSize(newRootRect->size);
		}
		GetCurrentComponent()->layout.rootRect = newRootRect;
	}

	void View::SetParentFillRatio(Size newParentFillRatio)
	{
		ClampSize(newParentFillRatio);
		GetCurrentComponent()->layout.parentFillRatio.X = newParentFillRatio;
	}

	void View::SetParentFillRatioSecondary(Size newParentFillRatioSecondary)
	{
		ClampSize(newParentFillRatioSecondary);
		GetCurrentComponent()->layout.parentFillRatio.Y = newParentFillRatioSecondary;
	}

	void View::SetCursor(std::optional<Cursor> newCursor)
	{
		GetCurrentComponent()->cursor = newCursor;
	}

	bool View::HasInputFocus() const
	{
		if (!inputFocus)
		{
			return false;
		}
		auto componentIndex = GetCurrentComponentIndex();
		while (auto nextComponentIndex = componentStore[*componentIndex].forwardInputFocusIndex)
		{
			componentIndex = *nextComponentIndex;
		}
		return componentIndex == inputFocus->component;
	}

	void View::GiveInputFocus()
	{
		SetInputFocus(GetCurrentComponentIndex());
	}

	bool View::IsHovered() const
	{
		return GetCurrentComponent()->hovered;
	}

	View::Size View::GetOverflow() const
	{
		return GetCurrentComponent()->overflow.X;
	}

	View::Size View::GetOverflowSecondary() const
	{
		return GetCurrentComponent()->overflow.Y;
	}

	View::Rect View::GetRect() const
	{
		return GetCurrentComponent()->rect;
	}

	bool View::IsMouseDown(MouseButtonIndex button) const
	{
		return componentMouseDownEvent && GetCurrentComponentIndex() == componentMouseDownEvent->component && componentMouseDownEvent->button == button;
	}

	bool View::IsClicked(MouseButtonIndex button) const
	{
		return componentMouseClickEvent && GetCurrentComponentIndex() == componentMouseClickEvent->component && componentMouseClickEvent->button == button;
	}

	std::optional<View::Pos2> View::GetScrollDistance() const
	{
		if (componentMouseScrollEvent && GetCurrentComponentIndex() == componentMouseScrollEvent->component)
		{
			return componentMouseScrollEvent->distance;
		}
		return std::nullopt;
	}
}
