#pragma once

#include "Component.h"

#include "TextWrapper.h"
#include "Color.h"

#include <utility>
#include <functional>

namespace gui
{
	class TextBox : public Component
	{
	public:
		using ChangeCallback = std::function<void ()>;
		using ValidateCallback = std::function<void ()>;
		using FilterCallback = std::function<bool (int)>;
		using FormatCallback = std::function<String (const String &)>;

	private:
		ChangeCallback change;
		ValidateCallback validate;
		FilterCallback filter;
		FormatCallback format;
		String text;
		int cursorPos;
		int selectionPos;
		int selectionSize;
		int selection0;
		int selection1;
		int limit = 0;
		bool multiline = false;
		bool acceptNewline = false;
		bool allowCopy = true;
		bool editable = true;
		bool drawBody = true;
		Point scrollOffset;
		TextWrapper::PointLine cursorUpDown;
		bool cursorUpDownValid;
		Color textColor;
		Rect textRect = Rect{ Point{ 0, 0 }, Point{ 0, 0 } };
		String placeHolder;

		TextWrapper textWrapper;
		TextWrapper::PointLine wrapperCursor;
		TextWrapper::PointLine wrapperSelectionBegin;
		TextWrapper::PointLine wrapperSelectionEnd;
		String textToDraw;

		void UpdateWrapper();
		void UpdateText();
		void UpdateCursor();
		void Selection01();
		void Reset();
		int WordBorderRight() const;
		int WordBorderLeft() const;
		int ParagraphBorderRight() const;
		int ParagraphBorderLeft() const;
		void UpdateTextToDraw();
		void UpdateTextOffset(bool strict);
		String Filter(const String &toInsert);

	public:
		TextBox();
		void Tick() final override;
		virtual void Draw() const override;
		void MouseDragMove(Point current, int button) final override;
		void MouseDragBegin(Point current, int button) final override;
		void MouseDragEnd(Point current, int button) final override;
		void MouseClick(Point current, int button, int clicks) final override;
		bool KeyPress(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt) final override;
		bool TextInput(const String &data) final override;
		void TextEditing(const String &data) final override;
		
		void SelectAll();
		void Cut();
		void Copy();
		void Paste();

		void Change(ChangeCallback newChange)
		{
			change = newChange;
		}
		ChangeCallback Change() const
		{
			return change;
		}

		void Validate(ValidateCallback newValidate)
		{
			validate = newValidate;
		}
		ValidateCallback Validate() const
		{
			return validate;
		}

		void Filter(FilterCallback newFilter);
		FilterCallback Filter() const
		{
			return filter;
		}

		void Format(FormatCallback newFormat);
		FormatCallback Format() const
		{
			return format;
		}

		void Multiline(bool newMultiline);
		bool Multiline() const
		{
			return multiline;
		}

		virtual void Text(String newText);
		const String &Text() const
		{
			return text;
		}

		void Insert(const String &toInsert, int insertAt);
		void Remove(int removeFrom, int removeSize);

		void Selection(int newSelectionPos, int newSelectionSize);
		std::tuple<int, int> Selection() const
		{
			return std::make_tuple(selectionPos, selectionSize);
		}

		void Cursor(int newCursorPos);
		int Cursor() const
		{
			return cursorPos;
		}

		void Limit(int newLimit);
		int Limit() const
		{
			return limit;
		}

		void AcceptNewline(bool newAcceptNewline)
		{
			acceptNewline = newAcceptNewline;
		}
		bool AcceptNewline() const
		{
			return acceptNewline;
		}

		void AllowCopy(bool newAllowCopy)
		{
			allowCopy = newAllowCopy;
		}
		bool AllowCopy() const
		{
			return allowCopy;
		}

		void Editable(bool newEditable)
		{
			editable = newEditable;
		}
		bool Editable() const
		{
			return editable;
		}

		void DrawBody(bool newDrawBody)
		{
			drawBody = newDrawBody;
		}
		bool DrawBody() const
		{
			return drawBody;
		}

		virtual void Size(Point newSize) override;
		Point Size() const
		{
			return Component::Size();
		}

		int Lines() const;

		void TextColor(Color newTextColor)
		{
			textColor = newTextColor;
		}
		Color TextColor() const
		{
			return textColor;
		}

		void TextRect(Rect newTextRect);
		Rect TextRect() const
		{
			return textRect;
		}

		void PlaceHolder(String newPlaceHolder);
		const String &PlaceHolder() const
		{
			return placeHolder;
		}

		void FocusLose()
		{
			if (validate)
			{
				validate();
			}
		}
	};
}
