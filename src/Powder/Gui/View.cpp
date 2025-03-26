#include "View.hpp"
#include "ViewUtil.hpp"
#include "Activity/ConfirmQuit.hpp"
#include "Common/Div.hpp"
#include "Host.hpp"
#include "Stack.hpp"
#include "SdlAssert.hpp"
#include "graphics/VideoBuffer.h"
#include <algorithm>

namespace Powder::Gui
{
	bool View::HandleEvent(const SDL_Event &event)
	{
		{
			auto exitEvent = ClassifyExitEvent(event);
			if (exitEvent == ExitEventType::exit && !popupIndices.empty())
			{
				ClosePopupInternal(componentStore[popupIndices.back()]);
				return true;
			}
			auto dispositionFlags = GetDisposition();
			auto okBits = DispositionFlags(DispositionFlagsBase(dispositionFlags) & DispositionFlagsBase(DispositionFlags::okBits));
			auto okDisabled = okBits == DispositionFlags::okDisabled;
			auto okMissing  = okBits == DispositionFlags::okMissing ;
			auto cancelEffort = DispositionFlagsBase(dispositionFlags) & DispositionFlagsBase(DispositionFlags::cancelEffort);
			auto cancelMissing = DispositionFlagsBase(dispositionFlags) & DispositionFlagsBase(DispositionFlags::cancelMissing);
			if ((exitEvent == ExitEventType::exit ||
			    (exitEvent != ExitEventType::none && okMissing)) && !cancelEffort && Cancel())
			{
				return true;
			}
			if ((exitEvent == ExitEventType::ok   && !okMissing && !okDisabled) ||
			    (exitEvent != ExitEventType::none && cancelMissing))
			{
				Ok();
				return true;
			}
		}
		auto &g = GetHost();
		switch (event.type)
		{
		case SDL_MOUSEMOTION:
			SetMousePos(Pos2{ event.motion.x, event.motion.y });
			break;

		case SDL_MOUSEBUTTONUP:
			SetMousePos(Pos2{ event.button.x, event.button.y });
			if (handleButtonsIndex && componentMouseDownEvent &&
			    componentMouseDownEvent->component == *handleButtonsIndex &&
			    componentMouseDownEvent->button == event.button.button)
			{
				componentMouseClickEvent = componentMouseDownEvent;
			}
			componentMouseDownEvent.reset();
			break;

		case SDL_MOUSEBUTTONDOWN:
			SetMousePos(Pos2{ event.button.x, event.button.y });
			for (auto &index : popupIndices)
			{
				auto &component = componentStore[index];
				auto &internal = componentStore[GetContext<PopupContext>(component).layoutRootIndex];
				if (internal.hovered)
				{
					break;
				}
				ClosePopupInternal(component);
			}
			if (inputFocus && inputFocus->component != handleInputIndex)
			{
				SetInputFocus(std::nullopt);
			}
			if (handleInputIndex)
			{
				SetInputFocus(*handleInputIndex);
			}
			if (handleButtonsIndex)
			{
				componentMouseDownEvent = ComponentMouseButtonEvent{ *handleButtonsIndex, event.button.button };
				return true;
			}
			if (cancelWhenRootMouseDown && (!underMouseIndex ||
			                                (rootIndex && underMouseIndex && *underMouseIndex == *rootIndex)))
			{
				Cancel();
			}
			break;

		case SDL_MOUSEWHEEL:
#if SDL_VERSION_ATLEAST(2, 26, 0)
			SetMousePos(Pos2{ event.wheel.mouseX, event.wheel.mouseY });
#endif
			if (handleWheelIndex)
			{
				int32_t sign = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
				int32_t dx = event.wheel.x * sign;
				int32_t dy = event.wheel.y * sign;
				if (!componentMouseScrollEvent)
				{
					componentMouseScrollEvent = ComponentMouseWheelEvent{ *handleWheelIndex, { 0, 0 } };
				}
				componentMouseScrollEvent->distance.X += dx;
				componentMouseScrollEvent->distance.Y += dy;
				return true;
			}
			break;

		case SDL_TEXTINPUT:
			if (SDL_GetModState() & KMOD_GUI)
			{
				break;
			}
			if (inputFocus)
			{
				inputFocus->events.push_back(InputFocus::TextInputEvent{ event.text.text });
				return true;
			}
			break;

		case SDL_KEYDOWN:
			if (SDL_GetModState() & KMOD_GUI)
			{
				break;
			}
			if (!event.key.repeat &&
			    event.key.keysym.sym == SDLK_q &&
			    (event.key.keysym.mod & KMOD_CTRL) &&
			    !(event.key.keysym.mod & KMOD_ALT) &&
			    g.GetGlobalQuit() &&
			    allowGlobalQuit)
			{
				PushAboveThis(std::make_shared<Activity::ConfirmQuit>(host));
				return true;
			}
			if (inputFocus)
			{
				inputFocus->events.push_back(InputFocus::KeyDownEvent{
					event.key.keysym.sym,
					bool(event.key.repeat),
					bool(event.key.keysym.mod & KMOD_CTRL),
					bool(event.key.keysym.mod & KMOD_SHIFT),
					bool(event.key.keysym.mod & KMOD_ALT),
				});
				return true;
			}
			break;

		case SDL_WINDOWEVENT:
			switch (event.window.event)
			{
			case SDL_WINDOWEVENT_ENTER:
				HandleMouseEnter();
				break;

			case SDL_WINDOWEVENT_LEAVE:
				HandleMouseLeave();
				break;

			case SDL_WINDOWEVENT_SHOWN:
				HandleWindowShown();
				break;

			case SDL_WINDOWEVENT_HIDDEN:
				HandleWindowHidden();
				break;
			}
			break;
		}
		return false;
	}

	void View::RequestRepeatFrame()
	{
		shouldRepeatFrame = true;
	}

#if DebugGuiView
	void View::DebugDump(int32_t depth, Component &component)
	{
		std::ostringstream keySs;
		// for (auto ch : componentStore.DereferenceSpan(component.key))
		// {
		// 	keySs << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int(ch) & 0xFF) << ' ';
		// }
		keySs << componentStore.DereferenceSpan(component.key);
		Log(std::string(depth, ' '), "component: ", keySs.str(), ", size: ", component.rect.size.X, " ", component.rect.size.Y);
		for (auto &child : componentStore.GetChildRange(component))
		{
			DebugDump(depth + 1, child);
		}
	}
#endif

	void View::UpdateUnderMouse(Component &component)
	{
		if (component.handleMouse && component.rect.Contains(*mousePos)) // TODO-REDO_UI: clip to parent
		{
			auto index = GetComponentIndex(component);
			underMouseIndex = index;
			component.hovered = true;
			if (component.cursor)
			{
				cursor = *component.cursor;
			}
			if (component.handleButtons)
			{
				handleButtonsIndex = index;
			}
			if (component.handleWheel)
			{
				handleWheelIndex = index;
			}
			if (component.handleInput)
			{
				handleInputIndex = index;
			}
			for (auto &child : componentStore.GetChildRange(component))
			{
				if (child.layout.immaterial)
				{
					continue;
				}
				UpdateUnderMouse(child);
				if (child.hovered && !component.layout.layered)
				{
					break;
				}
			}
		}
	}

	void View::SetRendererUpOuter(bool newRendererUp)
	{
		if (rendererUp == newRendererUp)
		{
			return;
		}
		if (rendererUp && !newRendererUp)
		{
			ShrinkLayoutRootTextures(0);
		}
		rendererUp = newRendererUp;
		SetRendererUp(newRendererUp);
	}

	void View::SetRendererUp(bool newRendererUp)
	{
	}

	void View::HandleBeforeFrame()
	{
	}

	void View::HandleBeforeGui()
	{
	}

	void View::HandleAfterGui()
	{
	}

	void View::HandleAfterFrame()
	{
	}

	void View::HandleTick()
	{
	}

	View::~View()
	{
		for (auto *cursor : cursors)
		{
			if (cursor)
			{
				SDL_FreeCursor(cursor);
			}
		}
	}

	void View::UpdateCursor()
	{
		auto effectiveCursor = mousePos ? cursor : Cursor::arrow;
		auto index = int32_t(effectiveCursor);
		cursors.resize(index + 1, nullptr);
		if (!cursors[index])
		{
			cursors[index] = SdlAssertPtr(SDL_CreateSystemCursor(SDL_SystemCursor(index)));
		}
		SDL_SetCursor(cursors[index]);
	}

	bool View::HandleGui()
	{
#if DebugGuiView
		debugFindIterationCount = 0;
		debugRelinkIterationCount = 0;
#endif
		componentStore.BeginFrame();
		Assert(componentStack.empty());
		cancelWhenRootMouseDown = false;
		allowGlobalQuit = true;
		auto prevTitle = title;
		title.reset();
		underMouseIndex.reset();
		handleButtonsIndex.reset();
		handleWheelIndex.reset();
		handleInputIndex.reset();
		for (auto &component : componentStore)
		{
			component.hovered = false;
		}
		auto prevCursor = cursor;
		cursor = Cursor::arrow;
		if (mousePos)
		{
			for (auto &index : layoutRootIndices)
			{
				UpdateUnderMouse(componentStore[index]);
				if (underMouseIndex)
				{
					break;
				}
			}
		}
		if (componentMouseDownEvent)
		{
			auto &component = componentStore[componentMouseDownEvent->component];
			if (component.cursor)
			{
				cursor = *component.cursor;
			}
		}
		if (prevCursor != cursor)
		{
			UpdateCursor();
		}
		layoutRootIndices.clear();
		popupIndices.clear();
		nextLayoutRootTextureIndex = 0;
		Gui();
		Assert(componentStack.empty());
		prevSiblingIndex.reset();
		componentMouseClickEvent.reset();
		componentMouseScrollEvent.reset();
		if (componentStore.EndFrame())
		{
			shouldUpdateLayout = true;
		}
		if (prevTitle != title)
		{
			stack->RequestUpdateTitle();
		}
		ShrinkLayoutRootTextures(int32_t(layoutRootIndices.size()));
		auto *sdlRenderer = GetHost().GetRenderer();
		for (int32_t i = 1; i < int32_t(layoutRootTextures.size()); ++i)
		{
			SdlAssertZero(SDL_RenderCopy(sdlRenderer, layoutRootTextures[i], nullptr, nullptr));
		}
		for (auto &index : layoutRootIndices)
		{
			auto &component = componentStore[index];
			auto &rootRect = component.layout.rootRect;
			component.layout.minSize = { rootRect->size.X, rootRect->size.Y };
			component.layout.maxSize = { rootRect->size.X, rootRect->size.Y };
		}
		for (auto &component : componentStore)
		{
			if (component.prevLayout != component.layout)
			{
				component.prevLayout = component.layout;
				shouldUpdateLayout = true;
			}
			if (component.prevContent != component.content)
			{
				component.prevContent = component.content;
				RequestRepeatFrame();
			}
		}
#if DebugGuiView
# if DebugGuiViewStats
		{
			std::ostringstream ss;
#  if DebugCrossFrameItemStore
			ss << componentStore.debugAllocCount << " " << componentStore.debugFreeCount << " ";
#  endif
			ss << debugFindIterationCount << " " << debugRelinkIterationCount << " " << componentStore.keyData.size();
			Log("stats: ", ss.str());
		}
# endif
# if DebugGuiViewDump
		if (shouldDoDebugDump)
		{
			if (rootIndex)
			{
				DebugDump(0, componentStore[*rootIndex]);
			}
			else
			{
				Log("(no components)");
			}
			shouldDoDebugDump = false;
		}
# endif
#endif
		if (shouldUpdateLayout)
		{
			// TODO-REDO_UI: do layout only on the subset of the component tree that needs it
			UpdateLayout();
			shouldUpdateLayout = false;
			RequestRepeatFrame();
		}
		bool result = false;
		if (shouldRepeatFrame)
		{
			result = true;
			shouldRepeatFrame = false;
		}
		return result;
	}

	void View::SetMousePos(std::optional<Pos2> newMousePos)
	{
		mousePos = newMousePos;
	}

	std::optional<View::Pos2> View::GetMousePos() const
	{
		return mousePos;
	}

	void View::SetCancelWhenRootMouseDown(bool newExitWhenRootMouseDown)
	{
		cancelWhenRootMouseDown = newExitWhenRootMouseDown;
	}

	void View::SetAllowGlobalQuit(bool newAllowGlobalQuit)
	{
		allowGlobalQuit = newAllowGlobalQuit;
	}

	bool View::GetWantTextInput() const
	{
		return bool(inputFocus);
	}

	void View::UpdateTextInput()
	{
		auto oldWantTextInput = wantTextInput;
		wantTextInput = GetWantTextInput();
		if (wantTextInput && !oldWantTextInput)
		{
			GetHost().StartTextInput();
		}
		if (!wantTextInput && oldWantTextInput)
		{
			GetHost().StopTextInput();
		}
	}

	void View::SetInputFocus(std::optional<ComponentIndex> componentIndex)
	{
		if (componentIndex)
		{
			while (auto nextComponentIndex = componentStore[*componentIndex].forwardInputFocusIndex)
			{
				componentIndex = *nextComponentIndex;
			}
			inputFocus = InputFocus{ *componentIndex };
		}
		else
		{
			inputFocus.reset();
		}
		UpdateTextInput();
	}

	void View::HandleWindowHidden()
	{
		HandleMouseLeave();
		SetInputFocus(std::nullopt);
	}

	void View::HandleWindowShown()
	{
	}

	void View::HandleMouseEnter()
	{
		int32_t x = 0;
		int32_t y = 0;
		SDL_GetMouseState(&x, &y);
		float rx = 0;
		float ry = 0;
		// SDL2 quirk: anything to do with renderer scale, including SDL_RenderWindowToLogical, is wrong while a render target is active
		//             so ensure that one isn't active during this function
		SDL_RenderWindowToLogical(GetHost().GetRenderer(), x, y, &rx, &ry);
		SetMousePos(Pos2{ int32_t(rx), int32_t(ry) });
		UpdateCursor();
	}

	void View::HandleMouseLeave()
	{
		UpdateCursor();
		SetMousePos(std::nullopt);
		componentMouseClickEvent.reset();
		componentMouseDownEvent.reset();
		componentMouseScrollEvent.reset();
	}

	void View::SetOnTop(bool newOnTop)
	{
	}

	void View::SetOnTopOuter(bool newOnTop)
	{
		if (onTop == newOnTop)
		{
			return;
		}
		if (onTop && !newOnTop)
		{
			for (auto &index : popupIndices)
			{
				ClosePopupInternal(componentStore[index]);
			}
			HandleMouseLeave();
			HandleWindowHidden();
		}
		if (!onTop && newOnTop)
		{
			HandleWindowShown();
			HandleMouseEnter();
			UpdateCursor();
			stack->RequestUpdateTitle();
		}
		onTop = newOnTop;
		SetOnTop(newOnTop);
	}

	bool View::MayBeHandledExclusively(const SDL_Event &event)
	{
		switch (event.type)
		{
		case SDL_KEYDOWN:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEWHEEL:
			return true;
			break;
		}
		return false;
	}

	void View::Exit()
	{
		stack->Pop(this);
	}

	void View::PushAboveThis(std::shared_ptr<View> view)
	{
		stack->Push(view);
	}

	View::ExitEventType View::ClassifyExitEvent(const SDL_Event &event) const
	{
		switch (event.type)
		{
		case SDL_QUIT:
			if (!GetHost().GetFastQuit())
			{
				return ExitEventType::exit;
			}
			break;

		case SDL_KEYDOWN:
			if (!event.key.repeat)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_RETURN:
				case SDLK_RETURN2:
				case SDLK_KP_ENTER:
					return ExitEventType::ok;

				case SDLK_ESCAPE:
					return ExitEventType::exit;
				}
			}
			break;
		}
		return ExitEventType::none;
	}

	View::DispositionFlags View::GetDisposition() const
	{
		return DispositionFlags::okMissing;
	}

	void View::Ok()
	{
	}

	std::optional<std::string> View::GetOkText() const
	{
		return std::nullopt;
	}

	bool View::Cancel()
	{
		Exit();
		return true;
	}
}
