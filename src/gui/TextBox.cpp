#include "TextBox.h"

#include "SDLWindow.h"
#include "Appearance.h"
#include "TextWrapper.h"
#include "ButtonMenu.h"
#include "graphics/FontData.h"
#include "language/Language.h"

#include <SDL.h>

// * TODO-REDO_UI: composition
// * TODO-REDO_UI: non-cursor scrolling

namespace gui
{
	TextBox::TextBox() : textColor(Appearance::colors.inactive.idle.text)
	{
		HoverCursor(Cursor::cursorIbeam);
		WantTextInput(true);
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
		scrollOffset = { 0, 0 };
		cursorUpDown = TextWrapper::PointLine{ { 0, 0 }, 0 };
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
			textToDraw = wrapped.Substr(0, begin.wrappedIndex) + InvertString() + wrapped.Substr(begin.wrappedIndex, end.wrappedIndex - begin.wrappedIndex) + InvertString() + wrapped.Substr(end.wrappedIndex);
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
		usingPlaceHolder = false;
		auto formatted = format ? format(text) : text;
		if (!formatted.size())
		{
			formatted = CommonColorString(commonLightGray) + placeHolder;
			usingPlaceHolder = true;
		}
		textWrapper.Update(formatted, multiline, textRect.size.x - 6, initialInset);
		UpdateTextToDraw();
	}

	void TextBox::UpdateText()
	{
		cursorTick = SDLWindow::Ref().Ticks();
		UpdateWrapper();
		UpdateCursor();
		shouldTriggerChange = true;
	}

	void TextBox::TriggerChangeIfNeeded()
	{
		if (shouldTriggerChange)
		{
			shouldTriggerChange = false;
			TriggerChange();
		}
	}

	void TextBox::TriggerChange()
	{
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

	void TextBox::UpdateTextOffset(bool cursorIntoView)
	{
		if (multiline)
		{
			scrollOffset.x = 0;
			auto height = scrollTargetRect.size.y;
			if (cursorIntoView)
			{
				auto wantVisibleLo = wrapperCursor.point.y;
				auto wantVisibleHi = wrapperCursor.point.y + FONT_H;
				if (scrollOffset.y <        - wantVisibleLo) scrollOffset.y =        - wantVisibleLo;
				if (scrollOffset.y > height - wantVisibleHi) scrollOffset.y = height - wantVisibleHi;
			}
			auto maxOffset = Lines() * FONT_H - height;
			if (scrollOffset.y < -maxOffset) scrollOffset.y = -maxOffset;
			if (maxOffset < 0) scrollOffset.y = 0;
		}
		else
		{
			scrollOffset.y = 0;
			auto width = scrollTargetRect.size.x;
			if (cursorIntoView)
			{
				auto wantVisibleLo = wrapperCursor.point.x;
				auto wantVisibleHi = wrapperCursor.point.x;
				if (scrollOffset.x <       - wantVisibleLo) scrollOffset.x =       - wantVisibleLo;
				if (scrollOffset.x > width - wantVisibleHi) scrollOffset.x = width - wantVisibleHi;
			}
			auto maxOffset = textWrapper.Index2PointLine(textWrapper.IndexEnd()).point.x - width;
			if (scrollOffset.x < -maxOffset) scrollOffset.x = -maxOffset;
			if (maxOffset < 0)
			{
				switch (align & Alignment::horizBits)
				{
				case Alignment::horizLeft:
					scrollOffset.x = 0;
					break;

				case Alignment::horizCenter:
					scrollOffset.x = -maxOffset / 2;
					break;

				case Alignment::horizRight:
					scrollOffset.x = -maxOffset;
					break;
				}
			}
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
			g.DrawRect({ abs, size }, d.body);
		}
		if (drawBorder)
		{
			g.DrawRectOutline({ abs, size }, c.border);
		}
		{
#ifdef DEBUG
			if (g.drawContentRects)
			{
				g.DrawRectOutline({ abs + textRect.pos, textRect.size }, { 0, 255, 255, 128 });
				g.DrawRectOutline({ abs + scrollTargetRect.pos, scrollTargetRect.size }, { 255, 0, 255, 128 });
			}
#endif
			SDLWindow::NarrowClipRect ncr({ abs + textRect.pos + Point{ 1, 1 }, textRect.size - Point{ 2, 2 } });
			auto textAbs = abs + scrollTargetRect.pos + scrollOffset;
			if (selectionSize)
			{
				auto width = textRect.size.x;
				for (auto l = wrapperSelectionBegin.line; l <= wrapperSelectionEnd.line; ++l)
				{
					auto left = (l == wrapperSelectionBegin.line) ? wrapperSelectionBegin.point.x : 0;
					auto right = (l == wrapperSelectionEnd.line) ? wrapperSelectionEnd.point.x : (width - 5);
					g.DrawRect({ textAbs + Point{ left - 1, l * FONT_H }, { right - left + 1, FONT_H } }, e.border);
				}
			}
			g.DrawText(textAbs, textToDraw, textColor, Enabled() ? 0 : SDLWindow::drawTextDarken);
			if (drawCursor && WithFocus() && (g.Ticks() - cursorTick) % 1000 < 500)
			{
				auto cur = textAbs + Point{ wrapperCursor.point.x, wrapperCursor.line * FONT_H };
				g.DrawLine(cur, cur + Point{ 0, FONT_H - 1 }, e.border);
			}
		}
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
		auto adjustSelection = [this, prevCursorPos, shift]() {
			if (shift)
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

		if (alt)
		{
			return false;
		}

		switch (sym)
		{
		case SDLK_UP:
			if (ctrl)
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
			if (ctrl)
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
			if (ctrl)
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
			if (ctrl)
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

		case SDLK_INSERT:
			if (ctrl && !shift)
			{
				if (allowCopy && !repeat)
				{
					Copy();
				}
				return true;
			}
			if (!ctrl && shift)
			{
				Paste();
				return true;
			}
			break;

		case SDLK_DELETE:
			if (shift)
			{
				if (allowCopy && !repeat)
				{
					Cut();
				}
			}
			else
			{
				shouldTriggerChange = false;
				if (selectionSize)
				{
					TextInput("");
				}
				else if (editable)
				{
					int toRemove = 1;
					if (ctrl)
					{
						toRemove = WordBorderRight() - cursorPos;
					}
					Remove(cursorPos, toRemove);
				}
				UpdateTextOffset(true);
				TriggerChangeIfNeeded();
			}
			return true;

		case SDLK_BACKSPACE:
			if (!shift)
			{
				shouldTriggerChange = false;
				if (selectionSize)
				{
					TextInput("");
				}
				else if (editable)
				{
					int toRemove = 1;
					if (ctrl)
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
				TriggerChangeIfNeeded();
			}
			return true;

		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			if (acceptNewline)
			{
				TextInput("\n");
				return true;
			}
			return false;

		case SDLK_a:
			if (ctrl)
			{
				if (!repeat)
				{
					SelectAll();
				}
				return true;
			}
			break;

		case SDLK_c:
			if (ctrl)
			{
				if (allowCopy && !repeat)
				{
					Copy();
				}
				return true;
			}
			break;

		case SDLK_v:
			if (ctrl)
			{
				Paste();
				return true;
			}
			break;

		case SDLK_x:
			if (ctrl)
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
			SDLWindow::ClipboardText(text.Substr(selectionPos, selectionSize));
			TextInput("");
		}
	}
	
	void TextBox::Copy()
	{
		if (selectionSize)
		{
			SDLWindow::ClipboardText(text.Substr(selectionPos, selectionSize));
		}
	}
	
	void TextBox::Paste()
	{
		if (!SDLWindow::ClipboardEmpty())
		{
			TextInput(SDLWindow::ClipboardText());
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
		shouldTriggerChange = false;
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
		TriggerChangeIfNeeded();
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

	void TextBox::InitialInset(int newInitialInset)
	{
		initialInset = newInitialInset;
		UpdateWrapper();
	}

	void TextBox::Size(Point newSize)
	{
		Component::Size(newSize);
		TextRect({ { 0, 0 }, newSize });
	}

	int TextBox::Lines() const
	{
		return textWrapper.WrappedLines();
	}

	void TextBox::MouseDragMove(Point current, int button)
	{
		if (button == SDL_BUTTON_LEFT)
		{
			auto off = AbsolutePosition() + scrollOffset + scrollTargetRect.pos;
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
			auto off = AbsolutePosition() + scrollOffset + scrollTargetRect.pos;
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
			auto off = AbsolutePosition() + scrollOffset + scrollTargetRect.pos;
			cursorPos = textWrapper.Point2Index(current - off).clearIndex;
			selection1 = cursorPos;
			Selection01();
			CaptureMouse(false);
		}
	}
	
	void TextBox::MouseClick(Point current, int button, int clicks)
	{
		// * TODO-REDO_UI: Don't trust sdl's "clicks", it's stupid.
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
				options.push_back("DEFAULT_LS_TEXTBOX_CUT"_Ls());
				actions.push_back(actionCut);
				options.push_back("DEFAULT_LS_TEXTBOX_COPY"_Ls());
				actions.push_back(actionCopy);
			}
			if (!SDLWindow::ClipboardEmpty() && editable)
			{
				options.push_back("DEFAULT_LS_TEXTBOX_PASTE"_Ls());
				actions.push_back(actionPaste);
			}
			options.push_back("DEFAULT_LS_TEXTBOX_SELECT_ALL"_Ls());
			actions.push_back(actionSelectAll);
			auto *bm = SDLWindow::Ref().EmplaceBack<ButtonMenu>().get();
			bm->Options(options);
			bm->ButtonSize({ 60, 16 });
			bm->SensiblePosition(current);
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
		ScrollTargetRect({ textRect.pos + Point{ 3, 3 }, textRect.size - Point{ 7, 5 } });
		if (oldTextRect.size.x != newTextRect.size.x)
		{
			UpdateWrapper();
		}
	}

	void TextBox::ScrollTargetRect(Rect newScrollTargetRect)
	{
		scrollTargetRect = newScrollTargetRect;
		UpdateTextOffset(false);
	}

	void TextBox::PlaceHolder(String newPlaceHolder)
	{
		placeHolder = newPlaceHolder;
		UpdateWrapper();
	}
}
