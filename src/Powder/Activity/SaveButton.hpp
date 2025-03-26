#pragma once
#include <utility>
#include <optional>
#include "Thumbnail.hpp"
#include "Common/Rect.hpp"
#include "Gui/Ticks.hpp"
#include "common/String.h"

namespace Powder::Gui
{
	class View;
}

namespace Powder::Activity
{
	struct BrowserItem
	{
		Thumbnail thumbnail;
		std::string title;
		bool selected = false;
		std::optional<Gui::Ticks> zoomBeganAt;
		struct OnlineInfo
		{
			ByteString author;
			int32_t votesUp;
			int32_t votesDown;
			bool published;
		};
		std::optional<OnlineInfo> onlineInfo;
	};

	std::pair<std::optional<Rect>, bool> GuiSaveButton(Gui::View &view, BrowserItem &item, bool selectable);
	void GuiSaveButtonZoom(Gui::View &view, Rect gridRect, Rect zoomRect, BrowserItem &item);
}
