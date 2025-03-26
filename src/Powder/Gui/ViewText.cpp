#include "View.hpp"
#include "ViewUtil.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"
#include "Common/Div.hpp"

#define DebugGuiViewText 0

namespace Powder::Gui
{
	namespace
	{
		constexpr View::Size selectionOverhang      = 1;
		constexpr View::Size placeholderTextPadding = 3;
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
		auto enabled = GetCurrentComponent()->prevContent.enabled;
		if (!enabled)
		{
			color.Alpha /= 2;
		}
		if ((TextFlagBase(textFlags) & TextFlagBase(TextFlags::selectable)) && enabled)
		{
			SetCursor(Cursor::ibeam);
			SetHandleButtons(true);
			SetHandleInput(true);
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
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::replaceWithDots))
		{
			context.stp.replaceWithDots = true;
		}
		auto alignmentRect = component.rect;
		for (AxisBase xyAxis = 0; xyAxis < 2; ++xyAxis)
		{
			// Using prevLayout so we have access to parameters set after this function returns.
			alignmentRect.pos % xyAxis += component.prevContent.textPaddingBefore % xyAxis;
			alignmentRect.size % xyAxis -= component.prevContent.textPaddingBefore % xyAxis + component.prevContent.textPaddingAfter % xyAxis;
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
			auto wrappedSize = wrapper.GetWrappedSize();
			if (!context.stp.maxWidth)
			{
				wrappedSize.X -= letterSpacing;
			}
			auto diff = alignmentRect.size - wrappedSize;
			// Using prevContent so we have access to parameters set after this function returns.
			// Using TruncateDiv causes some extremely weird results here and there, most notable in
			// the case of tool buttons, but this has been grandfathered in so whatever.
			auto offset = Pos2{
				TruncateDiv(diff.X * Size(parentComponent.prevContent.horizontalTextAlignment), 2),
				TruncateDiv(diff.Y * Size(parentComponent.prevContent.verticalTextAlignment), 2),
			};
			return alignmentRect.pos + offset + Pos2{ 0, fontCapLine };
		};
#if DebugGuiViewText
		{
			auto cr = g.GetClipRect();
			g.SetClipRect(g.GetSize().OriginRect());
			g.DrawRect(alignmentRect, 0xFFFF00FF_argb);
			g.DrawRect(alignmentRect.Inset(1), 0xFFFF00FF_argb);
			g.SetClipRect(cr);
		}
#endif
		if (placeholderText && text.view.empty())
		{
			auto placeholderStp = context.stp;
			placeholderStp.text = g.InternText(*placeholderText);
			placeholderStp.replaceWithDots = false;
			if (placeholderStp.maxWidth)
			{
				placeholderStp.maxWidth->width -= 2 * placeholderTextPadding;
			}
			auto placeholderSt = g.ShapeText(placeholderStp);
			auto &placeholderStw = g.GetShapedTextWrapper(placeholderSt);
			auto placeholderTextPos = getTextPos(placeholderStw);
			placeholderTextPos.X += placeholderTextPadding;
			g.DrawText(placeholderTextPos, placeholderSt, 0x80FFFFFF_argb);
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
				Rect rect{ wrapperPos + curPos - Pos2{ 0, 1 }, { 1, fontTypeSize + 1 } };
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
