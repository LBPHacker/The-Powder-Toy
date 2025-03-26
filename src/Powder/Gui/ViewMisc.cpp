#include "View.hpp"
#include "ViewUtil.hpp"
#include "Host.hpp"
#include <numbers>

namespace Powder::Gui
{
	namespace
	{
		struct LcgParameters
		{
			uint32_t multiply;
			uint32_t add;
		} lcgParameters = { UINT32_C(2891336453), UINT32_C(1) };

		constexpr uint32_t LcgSkip(uint32_t lcg, uint32_t steps)
		{
			LcgParameters lcgSkipParameters = lcgParameters;
			for (int32_t i = 0; i < 32; ++i)
			{
				if ((steps & (UINT32_C(1) << i)) != UINT32_C(0))
				{
					lcg = lcg * lcgSkipParameters.multiply + lcgSkipParameters.add;
				}
				lcgSkipParameters.add *= lcgSkipParameters.multiply + UINT32_C(1);
				lcgSkipParameters.multiply *= lcgSkipParameters.multiply;
			}
			return lcg;
		}
	}

	void View::Separator(ComponentKey key, Rgba8 color)
	{
		Separator(key, 1, color);
	}

	void View::Separator(ComponentKey key, Size size, Rgba8 color)
	{
		BeginComponent(key);
		auto &component = *GetCurrentComponent();
		auto &g = GetHost();
		color.Red   /= 2;
		color.Green /= 2;
		color.Blue  /= 2;
		g.FillRect(component.rect, color);
		SetSize(size);
		EndComponent();
	}

	void View::TextSeparator(ComponentKey key, StringView text, Rgba8 color)
	{
		TextSeparator(key, text, GetHost().GetCommonMetrics().size, color);
	}

	void View::TextSeparator(ComponentKey key, StringView text, Size size, Rgba8 color)
	{
		BeginTextSeparator(key, text, size, color);
		EndTextSeparator();
	}

	void View::BeginTextSeparator(ComponentKey key, StringView text, Size size, Rgba8 color)
	{
		BeginHPanel(key);
		auto &parentComponent = *GetParentComponent();
		if (parentComponent.prevLayout.primaryAxis == Axis::vertical)
		{
			SetParentFillRatio(0);
			SetSize(size);
		}
		else
		{
			SetParentFillRatioSecondary(0);
			SetSizeSecondary(size);
		}
		SetSpacing(5);
		BeginVPanel("left");
		Separator("separator", color);
		EndPanel();
		BeginText("text", text, TextFlags::autoWidth, color);
		SetParentFillRatio(0);
		EndText();
		BeginVPanel("right");
		Separator("separator", color);
		EndPanel();
	}

	void View::EndTextSeparator()
	{
		EndPanel();
	}

	void View::Spinner(ComponentKey key, Size size, Rgba8 color)
	{
		ClampSize(size);
		constexpr auto pi2 = std::numbers::pi_v<float> * 2.f;
		auto arms = size * 4 / 5;
		BeginComponent(key);
		auto &g = GetHost();
		auto r = GetRect();
		auto center = r.pos + r.size / 2;
		auto outer = size * 4 / 10;
		auto inner = size * 5 / 10;
		auto phase = g.GetLastTick() * 15 / 1000;
		auto componentNoise = (int32_t(LcgSkip(UINT32_C(1633), uint32_t(GetCurrentComponentIndex()->value))) >> 16) & INT32_C(0xFFFF);
		for (int32_t i = 0; i < arms; ++i)
		{
			auto angle = pi2 * ((i + phase + componentNoise) % arms) / arms;
			auto iColor = color;
			iColor.Alpha = iColor.Alpha * i / arms;
			auto cos = std::cos(angle);
			auto sin = std::sin(angle);
			g.DrawLine(
				center + Pos2{ Pos(std::round(cos * outer)), Pos(std::round(sin * outer)) },
				center + Pos2{ Pos(std::round(cos * inner)), Pos(std::round(sin * inner)) },
				iColor
			);
		}
		EndComponent();
	}
}
