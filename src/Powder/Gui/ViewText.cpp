#include "View.hpp"
#include "ViewUtil.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"
#include "Common/Div.hpp"

namespace Powder::Gui
{
	namespace
	{
		constexpr int32_t selectionOverhang      = 1;
		constexpr int32_t placeholderTextPadding = 3;
	}

	void View::BeginText(ComponentKey key, StringView text, TextFlags textFlags, Rgba8 color)
	{
		BeginTextInternal(key, text, std::nullopt, std::nullopt, textFlags, color);
	}

	void View::BeginText(ComponentKey key, StringView text, StringView overflowText, TextFlags textFlags, Rgba8 color)
	{
		BeginTextInternal(key, text, overflowText, std::nullopt, textFlags, color);
	}

	void View::BeginTextInternal(
		ComponentKey key, 
		StringView text, 
		std::optional<StringView> overflowText, 
		std::optional<StringView> placeholderText, 
		TextFlags textFlags, 
		Rgba8 color
	)
	{
		BeginComponent(key);
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::selectable))
		{
			SetCursor(Cursor::ibeam);
			SetHandleButtons(true);
			SetHandleInput(true);
		}
		auto enabled = GetCurrentComponent()->prevContent.enabled;
		if (!enabled)
		{
			color.Alpha /= 2;
		}
		auto &g = GetHost();
		auto &parentComponent = *GetParentComponent();
		auto &component = *GetCurrentComponent();
		auto &context = SetContext<TextContext>(component);
		context.stp = { g.InternText(text), std::nullopt, parentComponent.prevContent.horizontalTextAlignment, component.prevContent.textFirstLineIndent };
		if (overflowText)
		{
			context.stp.overflowText = g.InternText(*overflowText);
		}
		auto alignmentRect = component.rect;
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			// Using prevLayout so we have access to parameters set after this function returns.
			alignmentRect.pos % psAxis += component.prevContent.textPaddingBefore % psAxis;
			alignmentRect.size % psAxis -= component.prevContent.textPaddingBefore % psAxis + component.prevContent.textPaddingAfter % psAxis;
		}
		ClampSize(alignmentRect.size);
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::multiline))
		{
			context.stp.maxWidth = ShapeTextParameters::MaxWidth{ alignmentRect.size.X, true };
		}
		else if (overflowText)
		{
			context.stp.maxWidth = ShapeTextParameters::MaxWidth{ alignmentRect.size.X, false };
		}
		auto st = g.ShapeText(context.stp);
		auto &stw = g.GetShapedTextWrapper(st);
		auto wrappedSize = stw.GetWrappedSize();
		if (!context.stp.maxWidth)
		{
			wrappedSize.X -= letterSpacing;
		}
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::autoWidth))
		{
			auto minSize = wrappedSize.X + component.prevContent.textPaddingBefore.X + component.prevContent.textPaddingAfter.X;
			if (parentComponent.prevLayout.primaryAxis == Axis::horizontal)
			{
				SetMinSize(minSize);
			}
			else
			{
				SetMinSizeSecondary(minSize);
			}
		}
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::autoHeight))
		{
			auto minSize = wrappedSize.Y + component.prevContent.textPaddingBefore.Y + component.prevContent.textPaddingAfter.Y;
			if (parentComponent.prevLayout.primaryAxis == Axis::vertical)
			{
				SetMinSize(minSize);
			}
			else
			{
				SetMinSizeSecondary(minSize);
			}
		}
		auto getTextPos = [&](const TextWrapper &wrapper) {
			auto contentSize = wrapper.GetContentSize();
			auto diff = alignmentRect.size - contentSize;
			// Using prevContent so we have access to parameters set after this function returns.
			// Using TruncateDiv causes some extremely weird results here and there, most notable in
			// the case of tool buttons, but this has been grandfathered in so whatever.
			auto offset = Pos2{
				TruncateDiv(diff.X * Size(parentComponent.prevContent.horizontalTextAlignment), 2),
				TruncateDiv(diff.Y * Size(parentComponent.prevContent.verticalTextAlignment), 2),
			};
			return alignmentRect.pos + offset;
		};
		if (placeholderText && text.view.empty())
		{
			auto placeholderStp = context.stp;
			placeholderStp.text = g.InternText(*placeholderText);
			if (placeholderStp.maxWidth)
			{
				placeholderStp.maxWidth->width -= 2 * placeholderTextPadding;
			}
			auto placeholderSt = g.ShapeText(placeholderStp);
			auto &placeholderStw = g.GetShapedTextWrapper(placeholderSt);
			auto placeholderColor = color;
			placeholderColor.Alpha /= 2;
			auto placeholderTextPos = getTextPos(placeholderStw);
			placeholderTextPos.X += placeholderTextPadding;
			g.DrawText(placeholderTextPos, placeholderSt, placeholderColor);
		}
		auto textPos = getTextPos(stw);
		g.DrawText(textPos, st, color);
		auto wrapperPos = textPos - Pos2(0, fontCapLine);
		context.selectionMax = stw.GetIndexEnd().clear;
		bool clearMouseDownPos = true;
		if (HasInputFocus())
		{
			{
				context.selectionBegin = std::clamp(context.selectionBegin, 0, context.selectionMax);
				context.selectionEnd = std::clamp(context.selectionEnd, 0, context.selectionMax);
			}
			auto cursorColor = 0xFFFFFFFF_argb;
			if (IsMouseDown(SDL_BUTTON_LEFT))
			{
				auto m = *GetMousePos();
				clearMouseDownPos = false;
				if (context.mouseDownPos)
				{
					context.selectionEnd = stw.ConvertPointToIndex(m - wrapperPos).clear;
				}
				else
				{
					context.mouseDownPos = m;
					context.selectionEnd = stw.ConvertPointToIndex(*context.mouseDownPos - wrapperPos).clear;
					context.selectionBegin = context.selectionEnd;
				}
			}
			{
				auto curPos = stw.ConvertClearToPoint(context.selectionEnd);
				Rect rect{ wrapperPos + curPos, { 1, fontTypeSize } };
				g.FillRect(rect, cursorColor);
			}
			if (context.selectionBegin != context.selectionEnd)
			{
				auto lo = std::min(context.selectionBegin, context.selectionEnd);
				auto hi = std::max(context.selectionBegin, context.selectionEnd);
				auto [ loPos, loLine ] = stw.ConvertClearToPointAndLine(lo);
				auto [ hiPos, hiLine ] = stw.ConvertClearToPointAndLine(hi);
				auto oldClipRect = g.GetClipRect();
				for (auto i = loLine; i <= hiLine; ++i)
				{
					Rect rect{ wrapperPos + Pos2(-selectionOverhang, i * fontTypeSize), { wrappedSize.X + 2 * selectionOverhang, fontTypeSize } };
					if (i == loLine)
					{
						rect.pos.X += loPos.X;
						rect.size.X -= loPos.X;
					}
					if (i == hiLine)
					{
						rect.size.X -= wrappedSize.X - hiPos.X + selectionOverhang;
					}
					g.SetClipRect(rect & oldClipRect);
					g.FillRect({ wrapperPos - Pos2(selectionOverhang, 0), wrappedSize + Pos2(2 * selectionOverhang, 0) }, cursorColor);
					g.DrawText(textPos, st, InvertColor(color.NoAlpha()).WithAlpha(255));
				}
				g.SetClipRect(oldClipRect);
			}
		}
		if (clearMouseDownPos)
		{
			context.mouseDownPos.reset();
		}
	}

	void View::EndText()
	{
		EndComponent();
	}

	void View::Text(StringView text, Rgba8 color)
	{
		Text(text, text, color);
	}

	void View::Text(ComponentKey key, StringView text, Rgba8 color)
	{
		BeginText(key, text, TextFlags::none, color);
		EndText();
	}

	int32_t View::TextGetWrappedLines() const
	{
		auto &g = GetHost();
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<TextContext>(component);
		auto st = g.ShapeText(context.stp);
		auto &stw = g.GetShapedTextWrapper(st);
		return stw.GetWrappedLines();
	}
}
