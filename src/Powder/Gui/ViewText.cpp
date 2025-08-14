#include "View.hpp"
#include "ViewUtil.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"
#include "Common/Div.hpp"

namespace Powder::Gui
{
	namespace
	{
		constexpr int32_t selectionOverhang = 1;
	}

	void View::BeginText(ComponentKey key, StringView text, TextFlags textFlags, Rgba8 color)
	{
		BeginComponent(key);
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::selectable))
		{
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
		ShapeTextParameters stp{ g.InternText(text), std::nullopt, parentComponent.prevContent.horizontalTextAlignment };
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
			stp.maxWidth = ShapeTextParameters::MaxWidth{ alignmentRect.size.X, true };
		}
		auto st = g.ShapeText(stp);
		auto &stw = g.GetShapedTextWrapper(st);
		auto wrappedSize = stw.GetWrappedSize();
		if (!stp.maxWidth)
		{
			wrappedSize.X -= letterSpacing;
		}
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::autoWidth))
		{
			if (parentComponent.prevLayout.primaryAxis == Axis::horizontal)
			{
				SetMinSize(wrappedSize.X);
			}
			else
			{
				SetMinSizeSecondary(wrappedSize.X);
			}
		}
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::autoHeight))
		{
			if (parentComponent.prevLayout.primaryAxis == Axis::vertical)
			{
				SetMinSize(wrappedSize.Y);
			}
			else
			{
				SetMinSizeSecondary(wrappedSize.Y);
			}
		}
		auto contentSize = stw.GetContentSize();
		auto diff = alignmentRect.size - contentSize;
		// Using prevContent so we have access to parameters set after this function returns.
		// Using TruncateDiv causes some extremely weird results here and there, most notable in
		// the case of tool buttons, but this has been grandfathered in so whatever.
		auto offset = Pos2{
			TruncateDiv(diff.X * Size(parentComponent.prevContent.horizontalTextAlignment), 2),
			TruncateDiv(diff.Y * Size(parentComponent.prevContent.verticalTextAlignment), 2),
		};
		auto textPos = alignmentRect.pos + offset;
		auto wrapperPos = textPos - Pos2(0, fontCapLine);
		g.DrawText(textPos, st, color);
		auto &context = SetContext<TextContext>(component);
		context.selectionMax = stw.GetIndexEnd().clear;
		bool clearMouseDownPos = true;
		if (HasInputFocus())
		{
			context.stp = stp;
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
}
