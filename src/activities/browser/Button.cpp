#include "Button.h"
#include "VoteBars.h"

#include "gui/SDLWindow.h"
#include "gui/Appearance.h"
#include "backend/Backend.h"
#include "backend/SaveInfo.h"
#include "gui/ImageButton.h"
#include "gui/Texture.h"
#include "gui/Icons.h"
#include "gui/ButtonMenu.h"
#include "gui/CheckBox.h"
#include "graphics/FontData.h"
#include "common/tpt-minmax.h"
#include "language/Language.h"

#include <SDL.h>
#include <iostream>

namespace activities::browser
{
	constexpr auto windowSize         = gui::Point{ WINDOWW, WINDOWH };
	constexpr auto borderNormal       = gui::Color{ 0xB4, 0xB4, 0xB4, 0xFF };
	constexpr auto borderHighlight    = gui::Color{ 0xFF, 0xFF, 0xFF, 0xFF };
	constexpr auto groupNormal        = gui::Color{ 0x64, 0x82, 0xA0, 0xFF };
	constexpr auto groupHighlight     = gui::Color{ 0xC8, 0xE6, 0xFF, 0xFF };
	constexpr auto hoverBackground    = gui::Color{ 0xFF, 0xFF, 0xFF, 0x20 };
	constexpr auto selectedBackground = gui::Color{ 0x64, 0x82, 0xA0, 0x60 };
	constexpr auto thumbnailSize      = gui::Point{ XRES, YRES } / 6;
	constexpr auto bigThumbnailSize   = gui::Point{ XRES, YRES } / 3;
	constexpr auto scaleBarDownTo     = thumbnailSize.y / 2;
	constexpr auto scaleBarUpTo       = thumbnailSize.y - scaleBarDownTo;
	constexpr auto bigThumbnailDelay  = 500; // * TODO-REDO_UI: Make configurable?

	class BigThumbnailPanel : public gui::Component
	{
	public:
		void Draw() const final override
		{
			auto abs = AbsolutePosition();
			auto size = Size();
			auto &c = gui::Appearance::colors.inactive.idle;
			gui::SDLWindow::Ref().DrawRect({ abs, size }, c.shadow);
			gui::SDLWindow::Ref().DrawRectOutline({ abs, bigThumbnailSize + gui::Point{ 2, 2 } }, borderNormal);
			gui::SDLWindow::Ref().DrawRectOutline({ abs, size }, borderNormal);
			gui::SDLWindow::Ref().DrawRectOutline({ abs - gui::Point{ 1, 1 }, size + gui::Point{ 2, 2 } }, c.shadow);
		};
	};

	Button::Button(String newName, String newGroup, int newScoreUp, int newScoreDown, bool newDrawLock, bool newDrawBars)
	{
		scoreUp = newScoreUp;
		scoreDown = newScoreDown;
		name = newName;
		group = newGroup;
		drawBars = newDrawBars;

		HoverCursor(gui::Cursor::cursorHand);

		Text(gui::IconString(gui::Icons::missing, { 1, -2 })); // * TODO-REDO_UI: Clock icon.
		{
			StringBuilder sb;
			auto score = scoreUp - scoreDown;
			auto bg = score > 0 ? gui::Appearance::voteUpBackground : gui::Appearance::voteDownBackground;
			auto fg = gui::Color{ 0xE0, 0xE0, 0xE0, 0xFF };
			using Ch = String::value_type;
			sb << gui::ColorString(bg) << Ch(0xE03B) << gui::StepBackString()
			   << gui::ColorString(fg) << Ch(0xE02D);
			for (auto ch : String::Build(score))
			{
				if (ch != '-')
				{
					ch += 0xDFFF;
				}
				sb << gui::ColorString(bg) << Ch(0xE03C) << gui::StepBackString()
				   << gui::ColorString(fg) << Ch(0xE02E) << gui::StepBackString()
				   << ch;
			}
			sb << gui::ColorString(bg) << gui::AlignString({ -1, 0 }) << Ch(0xE03A) << gui::StepBackString()
			   << gui::ColorString(fg) << gui::AlignString({ -1, 0 }) << Ch(0xE02C);
			scoreToDraw = sb.Build();
		}

		selectCheck = EmplaceChild<gui::CheckBox>().get();
		selectCheck->Size({ 14, 14 });
		selectCheck->Visible(false);
		selectCheck->Change([this](bool value) {
			ResetBigThumbnailCounting();
			selected = value;
			TriggerSelect();
		});

		if (newDrawLock)
		{
			name = gui::ColorString({ 0xFF, 0xFF, 0xFF, 0xFF }) + gui::IconString(gui::Icons::lockOutline, { 0, -2 }) + gui::StepBackString() +
			       gui::ColorString({ 0xD4, 0x97, 0x51, 0xFF }) + gui::IconString(gui::Icons::lockBody   , { 0, -2 }) + gui::ResetString() +
			       gui::OffsetString(3) + name;
		}

		bigThumbnail = EmplaceChild<BigThumbnailPanel>().get();
		bigThumbnail->HoverCursor(gui::Cursor::cursorHand);
		auto size = bigThumbnailSize + gui::Point{ 2, 6 + 2 * FONT_H };
		bigThumbnail->Size(size);
		bigThumbnail->Visible(false);

		bigThumbnailButton = bigThumbnail->EmplaceChild<gui::ImageButton>().get();
		bigThumbnailButton->HoverCursor(gui::Cursor::cursorHand);
		bigThumbnailButton->Size(bigThumbnailSize);
		bigThumbnailButton->Position({ 1, 1 });
		bigThumbnailButton->Text(Text());
		bigThumbnailButton->Enabled(false);

		auto *nameStatic = bigThumbnail->EmplaceChild<gui::Static>().get();
		nameStatic->HoverCursor(gui::Cursor::cursorHand);
		nameStatic->Size({ bigThumbnailSize.x - 6, 15 });
		nameStatic->Position({ 4, bigThumbnailSize.y + 2 });
		nameStatic->Text(name);
		nameStatic->TextColor(borderNormal);
		nameStatic->Align(gui::Alignment::horizCenter | gui::Alignment::vertCenter);

		auto *groupStatic = bigThumbnail->EmplaceChild<gui::Static>().get();
		groupStatic->HoverCursor(gui::Cursor::cursorHand);
		groupStatic->Size({ bigThumbnailSize.x - 6, 15 });
		groupStatic->Position({ 4, bigThumbnailSize.y + 2 + FONT_H });
		groupStatic->Text(group);
		groupStatic->TextColor(groupNormal);
		groupStatic->Align(gui::Alignment::horizCenter | gui::Alignment::vertCenter);

		UpdateLayout();
	}

	void Button::Draw() const
	{
		auto &g = gui::SDLWindow::Ref();
		auto abs = AbsolutePosition();
		if (UnderMouse())
		{
			g.DrawRect({ abs, Size() }, hoverBackground);
		}
		if (selected)
		{
			g.DrawRect({ abs, Size() }, selectedBackground);
		}
		ImageButton::Draw();
		auto borderColor = borderNormal;
		auto nameColor = borderNormal;
		auto groupColor = groupNormal;
		if (UnderMouse())
		{
			borderColor = borderHighlight;
			if (GroupHover())
			{
				groupColor = groupHighlight;
			}
			else
			{
				nameColor = borderHighlight;
			}
		}
		g.DrawRectOutline({ abs + drawContentAt, thumbnailSize + gui::Point{ 2, 2 } }, borderColor);
		g.DrawText(abs + drawNameAt, nameToDraw, nameColor);
		if (drawBars)
		{
			auto drawBarsAt = abs + drawContentAt + gui::Point{ thumbnailSize.x + 1, 0 };
			g.DrawRectOutline({ drawBarsAt, { 7, thumbnailSize.y + 2 } }, borderColor);
			g.DrawRect(                   { drawBarsAt + gui::Point{ 1, 1                              }, { 5, scaleBarUpTo   } }, gui::Appearance::voteUpBackground  );
			g.DrawRect(                   { drawBarsAt + gui::Point{ 1, 1 + scaleBarUpTo               }, { 5, scaleBarDownTo } }, gui::Appearance::voteDownBackground);
			if (barHeightUp  ) g.DrawRect({ drawBarsAt + gui::Point{ 2, 1 + scaleBarUpTo - barHeightUp }, { 3, barHeightUp    } }, gui::Appearance::voteUp            );
			if (barHeightDown) g.DrawRect({ drawBarsAt + gui::Point{ 2, 1 + scaleBarUpTo               }, { 3, barHeightDown  } }, gui::Appearance::voteDown          );
			g.DrawText(abs + drawScoreAt, scoreToDraw, { 0xFF, 0xFF, 0xFF, 0xFF });
		}
		g.DrawText(abs + drawGroupAt, groupToDraw, groupColor);
	}

	void Button::ImageData(gui::Image newImage)
	{
		auto bigImage = newImage;
		bigImage.Size(bigThumbnailSize, gui::Image::resizeModeLinear);
		bigThumbnailButton->Background()->ImageData(bigImage);
		newImage.Size(thumbnailSize, gui::Image::resizeModeLinear);
		Background()->ImageData(newImage);
	}

	void Button::Tick()
	{
		if (bigThumbnail)
		{
			selectCheck->Visible(UnderMouse());
			auto bigThumbnailVisible = UnderMouse() && bigThumbnailCountingSince + bigThumbnailDelay <= gui::SDLWindow::Ref().EventTimestamp();
			auto prevBigThumbnailVisible = bigThumbnail->Visible();
			bigThumbnail->Visible(bigThumbnailVisible);
			if (bigThumbnailVisible && !prevBigThumbnailVisible)
			{
				auto abs = AbsolutePosition();
				auto wantPosition = abs + drawThumbnailAt + thumbnailSize / 2 - bigThumbnailSize / 2 - bigThumbnailButton->Position();
				bigThumbnail->Position(gui::Rect{ { 2, 2 }, windowSize - bigThumbnail->Size() - gui::Point{ 3, 3 } }.Clamp(wantPosition) - abs);
				ChildToFront(bigThumbnail);
			}
		}
	}

	void Button::UpdateLayout()
	{
		auto contentToThumbnail = gui::Point{ 1, 1 };
		auto contentSize = thumbnailSize + gui::Point{ 2, 2 + 2 * FONT_H };
		auto contentRect = gui::Rect{ (Size() - contentSize) - (Size() - contentSize) / 2, contentSize };
		if (drawBars)
		{
			contentRect.pos.x -= 3;
		}
		drawContentAt = contentRect.pos;
		auto thumbnailRect = gui::Rect{ contentRect.pos + contentToThumbnail, thumbnailSize };
		drawThumbnailAt = thumbnailRect.pos;
		TextRect(thumbnailRect);
		BackgroundRect(thumbnailRect);
		auto fitText = [this](gui::Point &drawAt, String &toDraw, const String &source, int y) {
			gui::Point ts;
			gui::SDLWindow::Ref().TruncateText(toDraw, ts, source, Size().x - 10, "...");
			drawAt = gui::Point{ (Size().x - ts.x) - (Size().x - ts.x) / 2, y };
		};
		fitText(drawNameAt, nameToDraw, name, contentRect.pos.y + contentRect.size.y - 2 * FONT_H + 1);
		fitText(drawGroupAt, groupToDraw, group, contentRect.pos.y + contentRect.size.y - FONT_H + 1);
		VoteBars::ScaleVoteBars(barHeightUp, barHeightDown, scaleBarUpTo - 1, scaleBarDownTo - 1, scoreUp, scoreDown);
		{
			auto ts = gui::SDLWindow::Ref().TextSize(scoreToDraw);
			drawScoreAt = thumbnailRect.pos + thumbnailRect.size - gui::Point{ 1 + ts.x, 2 + ts.y };
		}
		selectCheck->Position(thumbnailRect.pos + gui::Point{ thumbnailRect.size.x - 15, 2 });
	}

	bool Button::GroupHover() const
	{
		return showGroup && gui::SDLWindow::Ref().MousePosition().y > AbsolutePosition().y + drawGroupAt.y;
	}

	void Button::Size(gui::Point newSize)
	{
		ImageButton::Size(newSize);
		UpdateLayout();
	}

	void Button::HandleClick(int button)
	{
		auto &g = gui::SDLWindow::Ref();
		if (button == SDL_BUTTON_LEFT)
		{
			if (GroupHover())
			{
				showGroup();
			}
			else
			{
				gui::Button::HandleClick(SDL_BUTTON_LEFT);
			}
		}
		if (button == SDL_BUTTON_RIGHT && contextMenu)
		{
			auto entries = contextMenu();
			if (entries.size())
			{
				auto *bm = g.EmplaceBack<gui::ButtonMenu>().get();
				std::vector<String> texts;
				std::vector<ContextMenuEntryCallback> funcs;
				int maxWidth = 0;
				for (auto &entry : entries)
				{
					texts.push_back(entry.text);
					funcs.push_back(entry.func);
					auto ts = g.TextSize(entry.text);
					if (maxWidth < ts.x)
					{
						maxWidth = ts.x;
					}
				}
				bm->Options(texts);
				bm->ButtonSize({ maxWidth + 20, 16 });
				bm->SensiblePosition(g.MousePosition());
				bm->Select([funcs](int index) {
					if (funcs[index])
					{
						funcs[index]();
					}
				});
			}
		}
		if (button == SDL_BUTTON_MIDDLE)
		{
			Selected(!selected);
			TriggerSelect();
		}
	}

	void Button::TriggerSelect()
	{
		if (select)
		{
			select(selected);
		}
	}

	void Button::Selected(bool newSelected)
	{
		selected = newSelected;
		selectCheck->Value(selected);
	}

	void Button::ResetBigThumbnailCounting()
	{
		bigThumbnailCountingSince = gui::SDLWindow::Ref().EventTimestamp();
	}

	void Button::MouseEnter(gui::Point current)
	{
		ResetBigThumbnailCounting();
	}

	bool Button::MouseMove(gui::Point current, gui::Point delta)
	{
		ResetBigThumbnailCounting();
		return false;
	}

	void Button::Text(const String &newText)
	{
		ImageButton::Text(newText);
		if (bigThumbnailButton)
		{
			bigThumbnailButton->Text(newText);
		}
	}
}
