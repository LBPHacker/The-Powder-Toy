#include "View.hpp"
#include "Host.hpp"

namespace Powder::Gui
{
	void View::Separator(ComponentKey key, Rgba8 color)
	{
		Separator(key, 1, color);
	}

	void View::Separator(ComponentKey key, int32_t size, Rgba8 color)
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
		TextSeparator(key, text, 17, color);
	}

	void View::TextSeparator(ComponentKey key, StringView text, int32_t size, Rgba8 color)
	{
		BeginTextSeparator(key, text, size, color);
		EndTextSeparator();
	}

	void View::BeginTextSeparator(ComponentKey key, StringView text, int32_t size, Rgba8 color)
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
}
