#include "TextBox.h"

#include "SDLWindow.h"
#include "Appearance.h"
#include "TextWrapper.h"
#include "ButtonMenu.h"
#include "graphics/FontData.h"

// * TODO-REDO_UI: composition
// * TODO-REDO_UI: non-cursor scrolling

namespace gui
{
	TextBox::TextBox() : textColor(Appearance::colors.inactive.idle.text)
	{
		Reset();
		UpdateText();
	}

	void TextBox::Reset()
	{
		text.clear();
		cursorPos = 0;
		selectionPos = 0;
		selectionSize = 0;
		selection0 = 0;
		selection1 = 0;
		scrollOffset = Point{ 0, 0 };
		cursorUpDown = TextWrapper::PointLine{ Point{ 0, 0 }, 0 };
		cursorUpDownValid = false;
	}

	void TextBox::Multiline(bool newMultiline)
	{
		Reset();
		multiline = newMultiline;
		UpdateText();
	}

	void TextBox::Text(String newText)
	{
		Reset();
		Insert(newText, 0);
	}

	void TextBox::Format(FormatCallback newFormat)
	{
		Reset();
		format = newFormat;
		UpdateText();
	}

	void TextBox::Filter(FilterCallback newFilter)
	{
		Reset();
		filter = newFilter;
		UpdateText();
	}

	void TextBox::UpdateTextToDraw()
	{
		wrapperCursor = textWrapper.Index2PointLine(textWrapper.Clear2Index(cursorPos));
		// * TODO-REDO_UI: maybe break textToDraw up into lines and draw those as needed
		if (selectionSize)
		{
			auto begin = textWrapper.Clear2Index(selectionPos);
			auto end = textWrapper.Clear2Index(selectionPos + selectionSize);
			auto &wrapped = textWrapper.WrappedText();
			textToDraw = wrapped.Substr(0, begin.wrappedIndex) + gui::InvertString() + wrapped.Substr(begin.wrappedIndex, end.wrappedIndex - begin.wrappedIndex) + gui::InvertString() + wrapped.Substr(end.wrappedIndex);
			wrapperSelectionBegin = textWrapper.Index2PointLine(begin);
			wrapperSelectionEnd = textWrapper.Index2PointLine(end);
		}
		else
		{
			textToDraw = textWrapper.WrappedText();
		}
	}

	void TextBox::UpdateCursor()
	{
		cursorUpDownValid = false;
		auto S = int(text.size());
		if (cursorPos     < 0) cursorPos     = 0;
		if (cursorPos     > S) cursorPos     = S;
		auto prevSelectionPos = selectionPos;
		if (selectionPos  < 0) selectionPos  = 0;
		if (selectionPos  > S) selectionPos  = S;
		selectionSize -= selectionPos - prevSelectionPos;
		auto M = S - selectionPos;
		if (selectionSize < 0) selectionSize = 0;
		if (selectionSize > M) selectionSize = M;
		UpdateTextToDraw();
	}

	void TextBox::Selection(int newSelectionPos, int newSelectionSize)
	{
		selectionPos = newSelectionPos;
		selectionSize = newSelectionSize;
		UpdateCursor();
	}

	void TextBox::Cursor(int newCursorPos)
	{
		cursorPos = newCursorPos;
		UpdateCursor();
	}

	void TextBox::UpdateWrapper()
	{
		auto formatted = format ? format(text) : text;
		if (!formatted.size())
		{
			formatted = gui::CommonColorString(gui::commonLightGray) + placeHolder;
		}
		textWrapper.Update(formatted, multiline, textRect.size.x - 6);
		UpdateTextToDraw();
	}

	void TextBox::UpdateText()
	{
		UpdateWrapper();
		UpdateCursor();
		if (change)
		{
			change();
		}
	}

	String TextBox::Filter(const String &toInsert)
	{
		String filtered;
		for (auto ch : toInsert)
		{
			if (ch < ' ' && ch != '\n' && ch != '\t')
			{
				continue;
			}
			if (filter && !filter(ch))
			{
				continue;
			}
			filtered.push_back(ch);
		}
		return filtered;
	}

	void TextBox::Insert(const String &toInsert, int insertAt)
	{
		auto S = int(text.size());
		if (insertAt < 0) insertAt = 0;
		if (insertAt > S) insertAt = S;
		text = text.Substr(0, insertAt) + Filter(toInsert) + text.Substr(insertAt);
		if (limit && int(text.size()) > limit)
		{
			text.erase(text.begin() + limit);
		}
		UpdateText();
	}

	void TextBox::Remove(int removeFrom, int removeSize)
	{
		auto S = int(text.size());
		auto prevRemoveFrom = removeFrom;
		if (removeFrom < 0) removeFrom = 0;
		if (removeFrom > S) removeFrom = S;
		removeSize -= removeFrom - prevRemoveFrom;
		auto M = S - removeFrom;
		if (removeSize < 0) removeSize = 0;
		if (removeSize > M) removeSize = M;
		text = text.Substr(0, removeFrom) + text.Substr(removeFrom + removeSize);
		UpdateText();
	}

	void TextBox::UpdateTextOffset(bool strict)
	{
		if (multiline)
		{
			auto height = textRect.size.y;
			if (strict)
			{
				auto wantVisibleLo = wrapperCursor.point.y;
				auto wantVisibleHi = wrapperCursor.point.y + FONT_H + 5;
				if (scrollOffset.y <        - wantVisibleLo) scrollOffset.y =        - wantVisibleLo;
				if (scrollOffset.y > height - wantVisibleHi) scrollOffset.y = height - wantVisibleHi;
				auto maxOffset = Lines() * FONT_H + 5 - height;
				if (scrollOffset.y < -maxOffset) scrollOffset.y = -maxOffset;
				if (maxOffset < 0) scrollOffset.y = 0;
			}
			auto maxOffset = Lines() * FONT_H + 5 - height;
			if (scrollOffset.y < -maxOffset) scrollOffset.y = -maxOffset;
			if (maxOffset < 0) scrollOffset.y = 0;
		}
		else
		{
			auto width = textRect.size.x;
			if (strict)
			{
				auto wantVisibleLo = wrapperCursor.point.x;
				auto wantVisibleHi = wrapperCursor.point.x + 7;
				if (scrollOffset.x <       - wantVisibleLo) scrollOffset.x =       - wantVisibleLo;
				if (scrollOffset.x > width - wantVisibleHi) scrollOffset.x = width - wantVisibleHi;
				auto maxOffset = textWrapper.Index2PointLine(textWrapper.IndexEnd()).point.x + 7 - width;
				if (scrollOffset.x < -maxOffset) scrollOffset.x = -maxOffset;
				if (maxOffset < 0) scrollOffset.x = 0;
			}
			auto maxOffset = textWrapper.Index2PointLine(textWrapper.IndexEnd()).point.x + 7 - width;
			if (scrollOffset.x < -maxOffset) scrollOffset.x = -maxOffset;
			if (maxOffset < 0) scrollOffset.x = 0;
		}
	}

	void TextBox::Tick()
	{
		UpdateTextOffset(Dragging(SDL_BUTTON_LEFT));
	}

	void TextBox::Draw() const
	{
		auto abs = AbsolutePosition();
		auto size = Size();
		auto &cc = Appearance::colors.inactive;
		auto &c = Enabled() ? (UnderMouse() ? cc.hover : cc.idle) : cc.disabled;
		auto &dd = Appearance::colors.inactive;
		auto &d = Enabled() ? (WithFocus() ? dd.hover : dd.idle) : dd.disabled;
		auto &ee = WithFocus() ? Appearance::colors.active : Appearance::colors.inactive;
		auto &e = Enabled() ? (UnderMouse() ? ee.hover : ee.idle) : ee.disabled;
		auto &g = SDLWindow::Ref();
		if (drawBody)
		{
			g.DrawRect(Rect{ abs, size }, d.body);
			g.DrawRectOutline(Rect{ abs, size }, c.border);
		}
		auto prevClip = g.ClipRect();
		g.ClipRect(prevClip.Intersect(Rect{ abs + textRect.pos + Point{ 1, 1 }, textRect.size - Point{ 2, 2 } }));
		auto textAbs = abs + textRect.pos + scrollOffset;
		if (selectionSize)
		{
			auto width = textRect.size.x;
			for (auto l = wrapperSelectionBegin.line; l <= wrapperSelectionEnd.line; ++l)
			{
				auto left = (l == wrapperSelectionBegin.line) ? wrapperSelectionBegin.point.x : 0;
				auto right = (l == wrapperSelectionEnd.line) ? wrapperSelectionEnd.point.x : (width - 5);
				g.DrawRect(Rect{ textAbs + Point{ left + 2, 3 + l * FONT_H }, Point{ right - left + 1, FONT_H } }, e.border);
			}
		}
		if (text.size() || !WithFocus()) // * Don't draw the placeholder if in focus.
		{
			g.DrawText(textAbs + Point{ 3, 3 }, textToDraw, textColor);
		}
		if (WithFocus())
		{
			auto cur = textAbs + Point{ 3 + wrapperCursor.point.x, 3 + wrapperCursor.line * FONT_H };
			g.DrawLine(cur, cur + Point{ 0, FONT_H - 1 }, e.border);
		}
		g.ClipRect(prevClip);
	}
	
	int TextBox::WordBorderRight() const
	{
		int curr = cursorPos;
		while (curr < int(text.size()) && TextWrapper::WordBreak(text[curr]))
		{
			curr += 1;
		}
		while (curr < int(text.size()) && !TextWrapper::WordBreak(text[curr]))
		{
			curr += 1;
		}
		return curr;
	}

	int TextBox::WordBorderLeft() const
	{
		int curr = cursorPos;
		while (curr - 1 >= 0 && TextWrapper::WordBreak(text[curr - 1]))
		{
			curr -= 1;
		}
		while (curr - 1 >= 0 && !TextWrapper::WordBreak(text[curr - 1]))
		{
			curr -= 1;
		}
		return curr;
	}

	int TextBox::ParagraphBorderRight() const
	{
		int curr = cursorPos;
		while (curr < int(text.size()) && text[curr] == '\n')
		{
			curr += 1;
		}
		while (curr < int(text.size()) && text[curr] != '\n')
		{
			curr += 1;
		}
		return curr;
	}

	int TextBox::ParagraphBorderLeft() const
	{
		int curr = cursorPos;
		while (curr - 1 >= 0 && text[curr - 1] == '\n')
		{
			curr -= 1;
		}
		while (curr - 1 >= 0 && text[curr - 1] != '\n')
		{
			curr -= 1;
		}
		return curr;
	}
	
	bool TextBox::KeyPress(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt)
	{
		auto prevCursorPos = cursorPos;
		auto adjustSelection = [this, prevCursorPos]() {
			if (SDLWindow::Ref().ModShift())
			{
				UpdateCursor();
				if (!selectionSize)
				{
					selection0 = prevCursorPos;
				}
				selection1 = cursorPos;
				Selection01();
			}
			else
			{
				selectionSize = 0;
				UpdateCursor();
			}
			UpdateTextOffset(true);
		};

		switch (sym)
		{
		case SDLK_UP:
			if (SDLWindow::Ref().ModCtrl())
			{
				// * TODO-REDO_UI: maybe scrolling?
			}
			else
			{
				if (multiline)
				{
					if (!cursorUpDownValid)
					{
						cursorUpDown = textWrapper.Index2PointLine(textWrapper.Clear2Index(cursorPos));
					}
					if (cursorUpDown.line > 0)
					{
						cursorUpDown.line -= 1;
						cursorUpDown.point.y -= FONT_H;
						cursorPos = textWrapper.Point2Index(cursorUpDown.point).clearIndex;
						adjustSelection();
						cursorUpDownValid = true;
					}
					return true;
				}
			}
			return false;

		case SDLK_DOWN:
			if (SDLWindow::Ref().ModCtrl())
			{
				// * TODO-REDO_UI: maybe scrolling?
			}
			else
			{
				if (multiline)
				{
					if (!cursorUpDownValid)
					{
						cursorUpDown = textWrapper.Index2PointLine(textWrapper.Clear2Index(cursorPos));
					}
					if (cursorUpDown.line < Lines() - 1)
					{
						cursorUpDown.line += 1;
						cursorUpDown.point.y += FONT_H;
						cursorPos = textWrapper.Point2Index(cursorUpDown.point).clearIndex;
						adjustSelection();
						cursorUpDownValid = true;
					}
					return true;
				}
			}
			return false;

		case SDLK_LEFT:
			if (SDLWindow::Ref().ModCtrl())
			{
				cursorPos = WordBorderLeft();
			}
			else
			{
				cursorPos -= 1;
			}
			adjustSelection();
			return true;

		case SDLK_RIGHT:
			if (SDLWindow::Ref().ModCtrl())
			{
				cursorPos = WordBorderRight();
			}
			else
			{
				cursorPos += 1;
			}
			adjustSelection();
			return true;

		case SDLK_HOME:
			cursorPos = 0;
			adjustSelection();
			return true;

		case SDLK_END:
			cursorPos = int(text.size());
			adjustSelection();
			return true;

		case SDLK_DELETE:
			if (selectionSize)
			{
				TextInput("");
			}
			else if (editable)
			{
				int toRemove = 1;
				if (SDLWindow::Ref().ModCtrl())
				{
					toRemove = WordBorderRight() - cursorPos;
				}
				Remove(cursorPos, toRemove);
			}
			UpdateTextOffset(true);
			return true;

		case SDLK_BACKSPACE:
			if (selectionSize)
			{
				TextInput("");
			}
			else if (editable)
			{
				int toRemove = 1;
				if (SDLWindow::Ref().ModCtrl())
				{
					auto left = WordBorderLeft();
					toRemove = cursorPos - left;
					cursorPos = left;
				}
				else
				{
					cursorPos -= 1;
				}
				Remove(cursorPos, toRemove);
			}
			UpdateTextOffset(true);
			return true;

		case SDLK_RETURN:
			if (acceptNewline)
			{
				TextInput("\n");
				return true;
			}
			return false;
		}

		switch (scan)
		{
		case SDL_SCANCODE_A:
			if (SDLWindow::Ref().ModCtrl())
			{
				if (!repeat)
				{
					SelectAll();
				}
				return true;
			}
			break;

		case SDL_SCANCODE_C:
			if (SDLWindow::Ref().ModCtrl())
			{
				if (allowCopy && !repeat)
				{
					Copy();
				}
				return true;
			}
			break;

		case SDL_SCANCODE_V:
			if (SDLWindow::Ref().ModCtrl())
			{
				Paste();
				return true;
			}
			break;

		case SDL_SCANCODE_X:
			if (SDLWindow::Ref().ModCtrl())
			{
				if (allowCopy && !repeat)
				{
					Cut();
				}
				return true;
			}
			break;
		}

		return false;
	}
	
	void TextBox::SelectAll()
	{
		cursorPos = int(text.size());
		selectionPos = 0;
		selectionSize = cursorPos;
		UpdateCursor();
	}

	void TextBox::Cut()
	{
		if (selectionSize)
		{
			SDLWindow::ClipboardText(text.Substr(selectionPos, selectionSize).ToUtf8());
			TextInput("");
		}
	}
	
	void TextBox::Copy()
	{
		if (selectionSize)
		{
			SDLWindow::ClipboardText(text.Substr(selectionPos, selectionSize).ToUtf8());
		}
	}
	
	void TextBox::Paste()
	{
		if (!SDLWindow::ClipboardEmpty())
		{
			TextInput(SDLWindow::ClipboardText().FromUtf8());
		}
	}

	void TextBox::Selection01()
	{
		auto lo = selection0;
		auto hi = selection1;
		if (hi < selection0) hi = selection0;
		if (lo > selection1) lo = selection1;
		selectionPos = lo;
		selectionSize = hi - lo;
		UpdateCursor();
	}

	bool TextBox::TextInput(const String &data)
	{
		if (!editable)
		{
			return false;
		}
		if (selectionSize)
		{
			Remove(selectionPos, selectionSize);
			cursorPos = selectionPos;
		}
		// * Best-effort attempt to remove banned characters. If this doesn't
		//   handle something I forgot to think of, Insert will take care of
		//   it when applying the filter, although the cursor may behave weirdly.
		auto toInsert = Filter(data);
		if (limit && int(toInsert.size()) + int(text.size()) > limit)
		{
			// * Best-effort attempt to trim the input to size. If this doesn't
			//   handle something I forgot to think of, Insert will take care of
			//   it when applying the limit, although the cursor may behave weirdly.
			toInsert = toInsert.Substr(0, limit - int(text.size()));
		}
		auto prevSize = int(text.size());
		Insert(toInsert, cursorPos);
		cursorPos += int(text.size()) - prevSize;
		selectionSize = 0;
		UpdateCursor();
		UpdateTextOffset(true);
		return true;
	}
	
	void TextBox::TextEditing(const String &data)
	{
		// * TODO-REDO_UI
	}

	void TextBox::Limit(int newLimit)
	{
		Reset();
		if (newLimit < 0)
		{
			newLimit = 0;
		}
		limit = newLimit;
	}

	void TextBox::Size(Point newSize)
	{
		Component::Size(newSize);
		TextRect(Rect({ Point{ 0, 0 }, newSize }));
	}

	int TextBox::Lines() const
	{
		return textWrapper.WrappedLines();
	}

	void TextBox::MouseDragMove(Point current, int button)
	{
		if (button == SDL_BUTTON_LEFT)
		{
			auto off = AbsolutePosition() + textRect.pos + scrollOffset + Point{ 3, 3 };
			cursorPos = textWrapper.Point2Index(current - off).clearIndex;
			selection1 = cursorPos;
			Selection01();
		}
	}
	
	void TextBox::MouseDragBegin(Point current, int button)
	{
		if (button == SDL_BUTTON_LEFT)
		{
			CaptureMouse(true);
			auto off = AbsolutePosition() + textRect.pos + scrollOffset + Point{ 3, 3 };
			cursorPos = textWrapper.Point2Index(current - off).clearIndex;
			selection0 = cursorPos;
			selection1 = cursorPos;
			Selection01();
		}
	}
	
	void TextBox::MouseDragEnd(Point current, int button)
	{
		if (button == SDL_BUTTON_LEFT)
		{
			auto off = AbsolutePosition() + textRect.pos + scrollOffset + Point{ 3, 3 };
			cursorPos = textWrapper.Point2Index(current - off).clearIndex;
			selection1 = cursorPos;
			Selection01();
			CaptureMouse(false);
		}
	}
	
	void TextBox::MouseClick(Point current, int button, int clicks)
	{
		if (button == SDL_BUTTON_LEFT)
		{
			if (clicks == 2)
			{
				cursorPos = WordBorderRight();
				selectionPos = WordBorderLeft();
				selectionSize = cursorPos - selectionPos;
				UpdateCursor();
			}
			if (clicks == 3)
			{
				cursorPos = ParagraphBorderRight();
				selectionPos = ParagraphBorderLeft();
				selectionSize = cursorPos - selectionPos;
				UpdateCursor();
			}
		}
		if (button == SDL_BUTTON_RIGHT)
		{
			enum Action
			{
				actionCut,
				actionCopy,
				actionPaste,
				actionSelectAll,
			};
			std::vector<String> options;
			std::vector<Action> actions;
			if (allowCopy && selectionSize)
			{
				options.push_back("Cut");
				actions.push_back(actionCut);
				options.push_back("Copy");
				actions.push_back(actionCopy);
			}
			if (!SDLWindow::ClipboardEmpty())
			{
				options.push_back("Paste");
				actions.push_back(actionPaste);
			}
			options.push_back("Select all");
			actions.push_back(actionSelectAll);
			auto bm = SDLWindow::Ref().EmplaceBack<ButtonMenu>();
			bm->Options(options);
			bm->ButtonSize(Point{ 60, 16 });
			bm->Position(current);
			bm->Select([this, actions](int index) {
				switch (actions[index])
				{
				case actionCut      : Cut();       break;
				case actionCopy     : Copy();      break;
				case actionPaste    : Paste();     break;
				case actionSelectAll: SelectAll(); break;
				}
			});
		}
	}

	void TextBox::TextRect(Rect newTextRect)
	{
		auto oldTextRect = textRect;
		textRect = newTextRect;
		if (oldTextRect.size.x != newTextRect.size.x)
		{
			UpdateWrapper();
		}
	}

	void TextBox::PlaceHolder(String newPlaceHolder)
	{
		placeHolder = newPlaceHolder;
		UpdateWrapper();
	}
}
