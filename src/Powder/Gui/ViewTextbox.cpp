#include "View.hpp"
#include "ViewUtil.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"
#include "Common/Defer.hpp"
#include "Common/Log.hpp"

namespace Powder::Gui
{
	namespace
	{
		constexpr Gui::View::Size multilinePadding = 1;
	}

	void View::BeginTextbox(ComponentKey key, std::string &str, TextboxFlags textboxFlags, Rgba8 color)
	{
		BeginTextboxInternal(key, str, std::nullopt, std::nullopt, textboxFlags, color);
	}

	void View::BeginTextbox(ComponentKey key, std::string &str, StringView placeholderText, TextboxFlags textboxFlags, Rgba8 color)
	{
		BeginTextboxInternal(key, str, std::nullopt, placeholderText, textboxFlags, color);
	}

	void View::BeginTextbox(ComponentKey key, std::string &str, StringView formattedText, StringView placeholderText, TextboxFlags textboxFlags, Rgba8 color)
	{
		BeginTextboxInternal(key, str, formattedText, placeholderText, textboxFlags, color);
	}

	void View::BeginTextboxInternal(
		ComponentKey key,
		std::string &str,
		std::optional<StringView> formattedText,
		std::optional<StringView> placeholderText,
		TextboxFlags textboxFlags,
		Rgba8 color
	)
	{
		BeginComponent(key);
		auto backgroundColor = 0xFFFFFFFF_argb;
		auto &g = GetHost();
		bool enabled;
		ExtendedSize2<MaxSize> maxSize;
		std::optional<int32_t> validLimit;
		{
			auto &component = *GetCurrentComponent();
			auto &context = SetContext<TextboxContext>(component);
			if (context.limit && *context.limit >= 0)
			{
				validLimit = context.limit;
			}
			if (validLimit && int32_t(str.size()) > *validLimit)
			{
				str = str.substr(0, *validLimit);
			}
			enabled = component.prevContent.enabled;
			maxSize = component.prevLayout.maxSize;
			if (!enabled)
			{
				backgroundColor.Alpha /= 2;
			}
			g.DrawRect(component.rect, backgroundColor);
		}
		auto &parentComponent = *GetParentComponent();
		if (parentComponent.prevLayout.primaryAxis == Axis::horizontal)
		{
			SetMaxSize(MaxSizeFitParent{});
		}
		else
		{
			SetMaxSizeSecondary(MaxSizeFitParent{});
		}
		if (enabled)
		{
			SetCursor(Cursor::ibeam);
		}
		auto textFlags = TextFlags::selectable;
		auto password = TextboxFlagBase(textboxFlags) & TextboxFlagBase(TextboxFlags::password);
		if (password)
		{
			textFlags = textFlags | TextFlags::replaceWithDots;
		}
		auto multiline = bool(TextboxFlagBase(textboxFlags) & TextboxFlagBase(TextboxFlags::multiline));
		if (multiline)
		{
			SetTextAlignment(Alignment::left, Alignment::top);
		}
		else
		{
			SetTextAlignment(Alignment::left, Alignment::center);
		}
		SetPadding(multilinePadding);
		BeginScrollpanel("multiline");
		{
			auto maxSizePrimary = maxSize % (AxisBase(parentComponent.prevLayout.primaryAxis) ^ 1);
			if (auto *size = std::get_if<Size>(&maxSizePrimary))
			{
				*size -= 2 * multilinePadding;
			}
			SetMaxSizeSecondary(maxSizePrimary);
		}
		SetPrimaryAxis(Axis::vertical);
		if (multiline)
		{
			textFlags = textFlags | TextFlags::autoHeight | TextFlags::multiline;
		}
		else
		{
			textFlags = textFlags | TextFlags::autoWidth;
		}
		{
			auto &component = *GetParentComponent();
			SetTextAlignment(component.prevContent.horizontalTextAlignment, component.prevContent.verticalTextAlignment);
		}
		BeginTextInternal("text", formattedText ? *formattedText : StringView(str), std::nullopt, placeholderText, textFlags, color);
		SetEnabled(enabled);
		auto &component = componentStore[(componentStack.end() - 3)->index];
		auto &context = GetContext<TextboxContext>(component);
		context.changed = false;
		context.textComponentIndex = *GetCurrentComponentIndex();
		component.forwardInputFocusIndex = context.textComponentIndex;
		auto &textContext = GetContext<TextContext>(*GetCurrentComponent());
		if (HasInputFocus())
		{
			// TODO-REDO_UI: scrolling
			// TODO-REDO_UI: word selection with double click
			// TODO-REDO_UI: paragraph selection with triple click
			auto &stp = textContext.stp;
			bool strChanged = true;
			ShapedTextIndex st;
			const TextWrapper *stw = nullptr;
			auto getSelectionMax = [&]() {
				return stw->GetIndexEnd().clear;
			};
			auto updateSt = [&]() {
				if (strChanged)
				{
					stp.text = g.InternText(str);
					st = g.ShapeText(stp);
					stw = &g.GetShapedTextWrapper(st);
					strChanged = false;
					textContext.selectionBegin = std::clamp(textContext.selectionBegin, 0, getSelectionMax());
					textContext.selectionEnd = std::clamp(textContext.selectionEnd, 0, getSelectionMax());
				}
			};
			updateSt();
			while (!inputFocus->events.empty())
			{
				Defer updateStLater([&] {
					updateSt();
				});
				auto getLo = [&]() {
					return std::min(textContext.selectionBegin, textContext.selectionEnd);
				};
				auto getHi = [&]() {
					return std::max(textContext.selectionBegin, textContext.selectionEnd);
				};
				auto replace = [&](TextWrapper::ClearIndex begin, TextWrapper::ClearIndex end, std::string_view insertStr) {
					// make sure this is the last thing done in the current iteration (e.g. continue;)
					auto beginIndex = stw->ConvertClearToIndex(begin);
					auto endIndex   = stw->ConvertClearToIndex(end  );
					auto beginRaw = formattedText ? beginIndex.unformatted : beginIndex.raw;
					auto endRaw   = formattedText ? endIndex  .unformatted : endIndex  .raw;
					if (endRaw > int32_t(str.size())) // formattedText does not necessarily align with str
					{
						return;
					}
					str.erase(beginRaw, endRaw - beginRaw);
					if (validLimit)
					{
						auto maxIstrSize = *validLimit - int32_t(str.size());
						if (int32_t(insertStr.size()) > maxIstrSize)
						{
							insertStr = insertStr.substr(0, maxIstrSize);
						}
					}
					str.insert(beginRaw, insertStr);
					for (size_t insertIt = 0; insertIt != insertStr.size(); insertIt += Utf8Next(insertStr, insertIt).length)
					{
						begin += 1;
					}
					textContext.selectionEnd = begin; // this is only an approximation for updateSt to correct
					textContext.selectionBegin = textContext.selectionEnd;
					strChanged = true;
				};
				auto copy = [&]() {
					auto beginIndex = stw->ConvertClearToIndex(getLo());
					auto endIndex   = stw->ConvertClearToIndex(getHi());
					auto beginRaw = formattedText ? beginIndex.unformatted : beginIndex.raw;
					auto endRaw   = formattedText ? endIndex  .unformatted : endIndex  .raw;
					GetHost().CopyTextToClipboard(str.substr(beginRaw, endRaw - beginRaw));
				};
				auto cut = [&]() {
					// make sure this is the last thing done in the current iteration (e.g. continue;)
					copy();
					replace(getLo(), getHi(), "");
				};
				auto paste = [&]() {
					// make sure this is the last thing done in the current iteration (e.g. continue;)
					if (auto text = GetHost().PasteTextFromClipboard())
					{
						replace(getLo(), getHi(), *text);
					}
				};
				auto nextClear = [&](int32_t direction) {
					return std::clamp(textContext.selectionEnd + direction, 0, getSelectionMax());
				};
				auto nextBorder = [&](int32_t direction) {
					auto isComplexSpace = [](UnicodeCodePoint codePoint) {
						switch (TextWrapper::ClassifyCharacter(codePoint))
						{
						case TextWrapper::CharacterClass::space:
						case TextWrapper::CharacterClass::newline:
							return true;

						default:
							break;
						}
						return false;
					};
					auto pos = textContext.selectionEnd;
					auto posIt = stw->ConvertClearToIndex(pos).raw;
					bool inWord = false;
					while (posIt + direction >= 0 && posIt + direction <= getSelectionMax())
					{
						Utf8Character ch;
						if (direction == -1)
						{
							ch = Utf8Prev(str, posIt);
						}
						else
						{
							ch = Utf8Next(str, posIt);
						}
						if (!ch.valid)
						{
							continue;
						}
						if (!isComplexSpace(ch.codePoint))
						{
							inWord = true;
						}
						if (inWord && isComplexSpace(ch.codePoint))
						{
							return pos;
						}
						posIt += TextWrapper::RawIndex(ch.length) * direction;
						pos += direction;
					}
					return pos;
				};
				auto adjustSelection = [&textContext](bool shift) {
					if (!shift)
					{
						textContext.selectionBegin = textContext.selectionEnd;
					}
				};
				auto &event = inputFocus->events.front();
				auto *keyDown = std::get_if<InputFocus::KeyDownEvent>(&event);
				auto upDown = false;
				Defer popEventLater([&]() {
					if (!upDown)
					{
						context.upDownPrev.reset();
					}
					if (strChanged)
					{
						context.changed = true;
					}
					inputFocus->events.pop_front();
				});
				if (keyDown && !keyDown->alt)
				{
					switch (keyDown->sym)
					{
					case SDLK_UP:
					case SDLK_DOWN:
						upDown = true;
						if (!context.upDownPrev)
						{
							context.upDownPrev = stw->ConvertClearToPoint(textContext.selectionEnd);
						}
						{
							context.upDownPrev->Y += (keyDown->sym == SDLK_DOWN ? 1 : -1) * fontTypeSize;
							textContext.selectionEnd = stw->ConvertPointToIndex(*context.upDownPrev).clear;
							adjustSelection(keyDown->shift);
							if (textContext.selectionEnd == 0 || textContext.selectionEnd == getSelectionMax())
							{
								context.upDownPrev.reset();
							}
						}
						break;

					case SDLK_LEFT:
						if (keyDown->ctrl)
						{
							textContext.selectionEnd = nextBorder(-1);
						}
						else
						{
							textContext.selectionEnd = nextClear(-1);
						}
						adjustSelection(keyDown->shift);
						break;

					case SDLK_RIGHT:
						if (keyDown->ctrl)
						{
							textContext.selectionEnd = nextBorder(1);
						}
						else
						{
							textContext.selectionEnd = nextClear(1);
						}
						adjustSelection(keyDown->shift);
						break;

					case SDLK_HOME:
						if (keyDown->ctrl)
						{
							textContext.selectionEnd = 0;
						}
						else
						{
							Log("nyi"); // TODO-REDO_UI: line edges (visually, so not just \n)
							break;
						}
						adjustSelection(keyDown->shift);
						break;

					case SDLK_END:
						if (keyDown->ctrl)
						{
							textContext.selectionEnd = getSelectionMax();
						}
						else
						{
							Log("nyi"); // TODO-REDO_UI: line edges (visually, so not just \n)
							break;
						}
						adjustSelection(keyDown->shift);
						break;

					case SDLK_INSERT:
						if (keyDown->ctrl && !keyDown->shift)
						{
							if (!keyDown->repeat && !password)
							{
								copy();
							}
						}
						if (!keyDown->ctrl && keyDown->shift)
						{
							paste();
							continue;
						}
						break;

					case SDLK_DELETE:
						if (keyDown->shift)
						{
							if (!keyDown->repeat && !password)
							{
								cut();
								continue;
							}
							break;
						}
						if (getLo() != getHi())
						{
							replace(getLo(), getHi(), "");
							continue;
						}
						if (keyDown->ctrl)
						{
							replace(getLo(), nextBorder(1), "");
							break;
						}
						replace(getLo(), nextClear(1), "");
						continue;

					case SDLK_BACKSPACE:
						if (getLo() != getHi())
						{
							replace(getLo(), getHi(), "");
							continue;
						}
						if (keyDown->ctrl)
						{
							replace(nextBorder(-1), getLo(), "");
							break;
						}
						replace(nextClear(-1), getLo(), "");
						continue;

					case SDLK_RETURN:
					case SDLK_KP_ENTER:
						if (multiline)
						{
							replace(getLo(), getHi(), "\n");
							continue;
						}
						break;

					case SDLK_a:
						if (keyDown->ctrl && !keyDown->repeat)
						{
							textContext.selectionEnd = getSelectionMax();
							textContext.selectionBegin = 0;
						}
						break;

					case SDLK_c:
						if (keyDown->ctrl && !keyDown->repeat && !password)
						{
							copy();
						}
						break;

					case SDLK_v:
						if (keyDown->ctrl)
						{
							paste();
							continue;
						}
						break;

					case SDLK_x:
						if (keyDown->ctrl && !keyDown->repeat && !password)
						{
							cut();
							continue;
						}
						break;
					}
				}
				if (auto *textInput = std::get_if<InputFocus::TextInputEvent>(&event))
				{
					replace(getLo(), getHi(), textInput->input);
					continue;
				}
			}
		}
		SetTextPadding(
			component.prevContent.textPaddingBefore.X,
			component.prevContent.textPaddingAfter.X,
			component.prevContent.textPaddingBefore.Y,
			component.prevContent.textPaddingAfter.Y
		);
		EndText();
		EndScrollpanel();
		SetTextPadding(4, 2);
	}

	void View::TextboxSelectAll()
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<TextboxContext>(component);
		auto &textContext = GetContext<TextContext>(componentStore[context.textComponentIndex]);
		textContext.selectionEnd = textContext.selectionMax;
		textContext.selectionBegin = 0;
	}

	void View::TextboxSetLimit(std::optional<int32_t> newLimit)
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<TextboxContext>(component);
		context.limit = newLimit;
	}

	bool View::EndTextbox()
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<TextboxContext>(component);
		bool changed = false;
		if (HasInputFocus())
		{
			changed = context.changed;
		}
		EndComponent();
		return changed;
	}

	bool View::Textbox(ComponentKey key, std::string &str, SetSizeSize size)
	{
		BeginTextbox(key, str, TextboxFlags::none);
		SetSize(size);
		return EndTextbox();
	}

	bool View::Textbox(ComponentKey key, std::string &str, StringView placeholderText, SetSizeSize size)
	{
		BeginTextbox(key, str, placeholderText, TextboxFlags::none);
		SetSize(size);
		return EndTextbox();
	}
}
