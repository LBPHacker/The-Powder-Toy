#include "View.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"
#include "Common/Log.hpp"

namespace Powder::Gui
{
	void View::BeginTextbox(ComponentKey key, std::string &str, TextboxFlags textboxFlags, Rgba8 color)
	{
		BeginComponent(key);
		auto backgroundColor = 0xFFFFFFFF_argb;
		auto &g = GetHost();
		{
			auto &component = *GetCurrentComponent();
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
		auto textFlags = TextFlags::selectable;
		if (TextboxFlagBase(textboxFlags) & TextboxFlagBase(TextboxFlags::multiline))
		{
			textFlags = textFlags | TextFlags::autoHeight | TextFlags::multiline;
			SetTextAlignment(Alignment::left, Alignment::top);
		}
		else
		{
			textFlags = textFlags | TextFlags::autoWidth;
			SetTextAlignment(Alignment::left, Alignment::center);
		}
		BeginText("text", str, textFlags, color);
		auto &component = *GetParentComponent();
		auto &context = SetContext<TextboxContext>(component);
		context.changed = false;
		context.textComponentIndex = *GetCurrentComponentIndex();
		component.forwardInputFocusIndex = context.textComponentIndex;
		auto &textContext = GetContext<TextContext>(*GetCurrentComponent());
		if (HasInputFocus())
		{
			// TODO-REDO_UI: scrolling
			// TODO-REDO_UI: word selection with double click
			// TODO-REDO_UI: paragraph selection with double click
			auto stp = *textContext.stp;
			bool strChanged = true;
			ShapedTextIndex st;
			const TextWrapper *stw = nullptr;
			auto getSelectionMax = [&]() {
				return stw->GetIndexEnd().clear;
			};
			while (!inputFocus->events.empty())
			{
				if (strChanged)
				{
					stp.text = g.InternText(str); // just in case in the first iteration; TODO-REDO_UI: figure out when this should matter
					st = g.ShapeText(stp);
					stw = &g.GetShapedTextWrapper(st);
					strChanged = false;
					textContext.selectionBegin = std::clamp(textContext.selectionBegin, 0, getSelectionMax());
					textContext.selectionEnd = std::clamp(textContext.selectionEnd, 0, getSelectionMax());
				}
				auto getLo = [&]() {
					return std::min(textContext.selectionBegin, textContext.selectionEnd);
				};
				auto getHi = [&]() {
					return std::max(textContext.selectionBegin, textContext.selectionEnd);
				};
				auto remove = [&](TextWrapper::ClearIndex begin, TextWrapper::ClearIndex end) {
					auto beginRaw = stw->ConvertClearToIndex(begin).raw;
					auto endRaw = stw->ConvertClearToIndex(end).raw;
					str.erase(beginRaw, endRaw - beginRaw);
					textContext.selectionEnd = begin; // this may be incorrect so don't rely on it anymore in this iteration
					textContext.selectionBegin = textContext.selectionEnd;
					strChanged = true;
				};
				auto replace = [&](TextWrapper::ClearIndex begin, TextWrapper::ClearIndex end, std::string_view istr) {
					remove(begin, end);
					// TODO-REDO_UI: remove disallowed code points from istr
					auto beginRaw = stw->ConvertClearToIndex(begin).raw;
					str.insert(beginRaw, istr);
					for (auto iit = Utf8Begin(istr), iend = Utf8End(istr); iit != iend; ++iit)
					{
						begin += 1;
					}
					textContext.selectionEnd = begin; // this may be incorrect so don't rely on it anymore in this iteration
					textContext.selectionBegin = textContext.selectionEnd;
					strChanged = true;
				};
				auto copy = [&]() {
					GetHost().CopyTextToClipboard(str.substr(getLo(), getHi() - getLo()));
				};
				auto cut = [&]() {
					copy();
					remove(getLo(), getHi());
				};
				auto paste = [&]() {
					if (auto text = GetHost().PasteTextFromClipboard())
					{
						replace(getLo(), getHi(), *text);
					}
				};
				auto nextClear = [&](int32_t direction) {
					return std::clamp(textContext.selectionEnd + direction, 0, getSelectionMax());
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
							Log("nyi"); // TODO-REDO_UI: word border
							break;
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
							Log("nyi"); // TODO-REDO_UI: word border
							break;
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
							if (!keyDown->repeat)
							{
								copy();
							}
						}
						if (!keyDown->ctrl && keyDown->shift)
						{
							paste();
						}
						break;

					case SDLK_DELETE:
						if (keyDown->shift)
						{
							if (!keyDown->repeat)
							{
								cut();
							}
							break;
						}
						if (getLo() != getHi())
						{
							remove(getLo(), getHi());
							break;
						}
						if (keyDown->ctrl)
						{
							Log("nyi"); // TODO-REDO_UI: word border
							break;
						}
						remove(getLo(), nextClear(1));
						break;

					case SDLK_BACKSPACE:
						if (getLo() != getHi())
						{
							remove(getLo(), getHi());
							break;
						}
						if (keyDown->ctrl)
						{
							Log("nyi"); // TODO-REDO_UI: word border
							break;
						}
						remove(nextClear(-1), getLo());
						break;

					case SDLK_RETURN:
					case SDLK_KP_ENTER:
						if (TextboxFlagBase(textboxFlags) & TextboxFlagBase(TextboxFlags::multiline))
						{
							replace(getLo(), getHi(), "\n");
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
						if (keyDown->ctrl && !keyDown->repeat)
						{
							copy();
						}
						break;

					case SDLK_v:
						if (keyDown->ctrl)
						{
							paste();
						}
						break;

					case SDLK_x:
						if (keyDown->ctrl && !keyDown->repeat)
						{
							cut();
						}
						break;
					}
				}
				if (auto *textInput = std::get_if<InputFocus::TextInputEvent>(&event))
				{
					replace(getLo(), getHi(), textInput->input);
				}
				if (!upDown)
				{
					context.upDownPrev.reset();
				}
				if (strChanged)
				{
					context.changed = true;
				}
				inputFocus->events.pop_front();
			}
		}
		SetTextPadding(4);
		EndText();
	}

	void View::TextboxSelectAll()
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<TextboxContext>(component);
		auto &textContext = GetContext<TextContext>(componentStore[context.textComponentIndex]);
		textContext.selectionEnd = textContext.selectionMax;
		textContext.selectionBegin = 0;
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
}
