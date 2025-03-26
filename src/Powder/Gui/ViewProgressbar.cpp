#include "View.hpp"
#include "ViewUtil.hpp"
#include "Format.h"
#include "Host.hpp"
#include "Colors.hpp"

namespace Powder::Gui
{
	namespace
	{
		Ticks indeterminatePeriod        = 2000;
		View::Size indeterminateWidthNum =    1;
		View::Size indeterminateWidthDen =    4;
	}

	void View::Progressbar(ComponentKey key, std::optional<Progress> progress, SetSizeSize size)
	{
		auto panel = ScopedVPanel(key);
		SetSize(size);
		SetLayered(true);
		std::optional<ByteString> percent;
		if (progress)
		{
			ClampSize(progress->numerator);
			ClampSize(progress->denominator);
			progress->denominator = std::max(1, progress->denominator);
			progress->numerator = std::clamp(progress->numerator, 0, progress->denominator);
			percent = ByteString::Build(Format::Precision(2), Format::Fixed(), float(progress->numerator) * 100.f / float(progress->denominator), "%");
		}
		auto &g = GetHost();
		auto r = GetRect();
		auto rr = r.Inset(2);
		if (percent)
		{
			Text("bottom", *percent);
			rr.size.X = rr.size.X * progress->numerator / progress->denominator;
			g.FillRect(rr, colorYellow.WithAlpha(255));
			auto oldClipRect = componentStack.back().clipRect;
			componentStack.back().clipRect &= rr;
			Text("top", *percent, 0xFF000000_argb);
			componentStack.back().clipRect = oldClipRect;
			g.SetClipRect(oldClipRect);
		}
		else
		{
			auto begin = Size(rr.size.X * (g.GetLastTick() % indeterminatePeriod) / indeterminatePeriod);
			auto end = begin + rr.size.X * indeterminateWidthNum / indeterminateWidthDen;
			if (end > rr.size.X)
			{
				g.FillRect(Rect{ rr.pos + Pos2{ begin, 0 }, { rr.size.X - begin, rr.size.Y } }, colorYellow.WithAlpha(255));
				g.FillRect(Rect{ rr.pos                   , { end - rr.size.X  , rr.size.Y } }, colorYellow.WithAlpha(255));
			}
			else
			{
				g.FillRect(Rect{ rr.pos + Pos2{ begin, 0 }, { end - begin, rr.size.Y } }, colorYellow.WithAlpha(255));
			}
		}
		g.DrawRect(r, 0xFFFFFFFF_argb);
	}
}
