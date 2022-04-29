#include "Spinner.h"

#include "SDLWindow.h"
#include "common/tpt-minmax.h"
#include "common/tpt-compat.h"

#include <cmath>

namespace gui
{
	constexpr float twopi = M_PI * 2.f;
	constexpr float innerRatio = 0.8f;
	constexpr int lowOpacity = 64;
	constexpr int highOpacity = 255;
	constexpr int lines = 24;

	void Spinner::Draw() const
	{
		auto abs = AbsolutePosition();
		auto size = Size();
		auto &g = SDLWindow::Ref();
		auto sweep = (g.EventTimestamp() % 1000) / 1000.f;
		for (auto i = 0; i < lines; ++i)
		{
			auto line = i / float(lines);
			auto diff = line - sweep;
			if (diff < 0)
			{
				diff += 1;
			}
			auto alpha = std::min(highOpacity, std::max(lowOpacity, int(diff * (highOpacity - lowOpacity) + lowOpacity)));
			auto nx = cosf(line * twopi);
			auto ny = sinf(line * twopi);
			g.DrawLine(
				abs + Point{ int((1.f + nx * innerRatio) * (size.x / 2.f) + 0.5f), int((1.f + ny * innerRatio) * (size.y / 2.f) + 0.5f) },
				abs + Point{ int((1.f + nx             ) * (size.x / 2.f) + 0.5f), int((1.f + ny             ) * (size.y / 2.f) + 0.5f) },
				{ 255, 255, 255, alpha }
			);
		}
	}
}
