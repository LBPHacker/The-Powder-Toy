#pragma once
#include "Config.h"

#include "gui/ImageButton.h"
#include "gui/Image.h"
#include "gui/ButtonMenu.h"
#include "gui/Timestamp.h"

#include <cstdint>

namespace gui
{
	class CheckBox;
}

namespace activities::browser
{
	class BigThumbnailPanel;

	class Button : public gui::ImageButton
	{
	public:
		using ContextMenuEntryCallback = std::function<void ()>;
		struct ContextMenuEntry
		{
			String text;
			ContextMenuEntryCallback func;
		};
		using ContextMenuCallback = std::function<std::vector<ContextMenuEntry> ()>;
		using SelectCallback = std::function<void (bool selected)>;
		using ShowGroupCallback = std::function<void ()>;

	private:
		ContextMenuCallback contextMenu;
		SelectCallback select;
		ShowGroupCallback showGroup;
		gui::CheckBox *selectCheck = nullptr;
		BigThumbnailPanel *bigThumbnail = nullptr;
		gui::ImageButton *bigThumbnailButton = nullptr;

		int scoreUp;
		int scoreDown;
		int barHeightUp;
		int barHeightDown;
		String name;
		String group;
		bool selected = false;
		bool drawBars = false;

		gui::Point drawContentAt, drawThumbnailAt, drawNameAt, drawGroupAt, drawScoreAt;
		String nameToDraw, groupToDraw, scoreToDraw;

		void UpdateLayout();
		bool GroupHover() const;

		gui::Timestamp bigThumbnailCountingSince = 0;
		void ResetBigThumbnailCounting();

	public:
		Button(String newName, String newGroup, int newScoreUp, int newScoreDown, bool newDrawLock, bool newDrawBars);

		void Tick() final override;
		void Draw() const final override;
		void MouseEnter(gui::Point current) final override;
		bool MouseMove(gui::Point current, gui::Point delta) final override;
		void HandleClick(int button) final override;

		void Size(gui::Point newSize) override;
		gui::Point Size() const
		{
			return Component::Size();
		}

		void ContextMenu(ContextMenuCallback newContextMenu)
		{
			contextMenu = newContextMenu;
		}
		ContextMenuCallback ContextMenu() const
		{
			return contextMenu;
		}

		void Select(SelectCallback newSelect)
		{
			select = newSelect;
		}
		SelectCallback Select() const
		{
			return select;
		}

		void ShowGroup(ShowGroupCallback newShowGroup)
		{
			showGroup = newShowGroup;
		}
		ShowGroupCallback ShowGroup() const
		{
			return showGroup;
		}

		void TriggerSelect();

		void Selected(bool newSelected);
		bool Selected() const
		{
			return selected;
		}

		void ImageData(gui::Image newImage);

		void Text(const String &newText) final override;
		const String &Text() const final override
		{
			return ImageButton::Text();
		}
	};
}
