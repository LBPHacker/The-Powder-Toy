#include "View.hpp"
#include "ViewUtil.hpp"
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
			if (exitWhenRootMouseDown && (!underMouseIndex ||
			                              (rootIndex && underMouseIndex && *underMouseIndex == *rootIndex)))
			{
				Exit();
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
		if (component.rect.Contains(*mousePos)) // TODO-REDO_UI: clip to parent
		{
			auto index = GetComponentIndex(component);
			underMouseIndex = index;
			component.hovered = true;
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
				UpdateUnderMouse(child);
				if (child.hovered)
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
		rendererUp = newRendererUp;
		SetRendererUp(newRendererUp);
	}

	void View::SetRendererUp(bool /* newRendererUp */)
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

	bool View::HandleGui()
	{
#if DebugGuiView
		debugFindIterationCount = 0;
		debugRelinkIterationCount = 0;
#endif
		componentStore.BeginFrame();
		Assert(componentStack.empty());
		exitWhenRootMouseDown = false;
		underMouseIndex.reset();
		handleButtonsIndex.reset();
		handleWheelIndex.reset();
		handleInputIndex.reset();
		if (mousePos && rootIndex)
		{
			UpdateUnderMouse(componentStore[*rootIndex]);
		}
		Gui();
		Assert(componentStack.empty());
		prevSiblingIndex.reset();
		componentMouseClickEvent.reset();
		componentMouseScrollEvent.reset();
		for (auto &component : componentStore)
		{
			component.hovered = false;
		}
		if (componentStore.EndFrame())
		{
			shouldUpdateLayout = true;
		}
		if (rootIndex)
		{
			auto &root = componentStore[*rootIndex];
			if (rootRect)
			{
				root.layout.minSize = { rootRect->size.X, rootRect->size.Y };
				root.layout.maxSize = { rootRect->size.X, rootRect->size.Y };
			}
			else
			{
				root.layout.minSize = { MinSizeFitChildren{}, MinSizeFitChildren{} };
				root.layout.maxSize = { MaxSizeInfinite{}, MaxSizeInfinite{} };
			}
			UpdateShouldUpdateLayout(root);
			UpdateShouldRepeatFrame(root);
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

	void View::UpdateShouldUpdateLayout(Component &component)
	{
		if (component.prevLayout != component.layout)
		{
			component.prevLayout = component.layout;
			shouldUpdateLayout = true;
		}
	}

	void View::UpdateShouldRepeatFrame(Component &component)
	{
		if (component.prevContent != component.content)
		{
			component.prevContent = component.content;
			RequestRepeatFrame();
		}
	}

	View::Size2 View::GetRootEffectiveSize() const
	{
		return rootEffectiveSize;
	}

	void View::SetMousePos(std::optional<Pos2> newMousePos)
	{
		mousePos = newMousePos;
	}

	std::optional<View::Pos2> View::GetMousePos() const
	{
		return mousePos;
	}

	void View::SetExitWhenRootMouseDown(bool newExitWhenRootMouseDown)
	{
		exitWhenRootMouseDown = newExitWhenRootMouseDown;
	}

	void View::SetInputFocus(std::optional<ComponentIndex> componentIndex)
	{
		if (inputFocus && !componentIndex)
		{
			GetHost().StopTextInput();
			inputFocus.reset();
		}
		if (componentIndex)
		{
			while (auto nextComponentIndex = componentStore[*componentIndex].forwardInputFocusIndex)
			{
				componentIndex = *nextComponentIndex;
			}
			if (!inputFocus)
			{
				GetHost().StartTextInput();
			}
			inputFocus = InputFocus{ *componentIndex };
		}
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
		int x = 0;
		int y = 0;
		SDL_GetMouseState(&x, &y);
		float rx = 0;
		float ry = 0;
		// SDL2 quirk: anything to do with renderer scale, including SDL_RenderWindowToLogical, is wrong while a render target is active
		//             so ensure that one isn't active during this function
		SDL_RenderWindowToLogical(GetHost().GetRenderer(), x, y, &rx, &ry);
		SetMousePos(Pos2{ int32_t(rx), int32_t(ry) });
	}

	void View::HandleMouseLeave()
	{
		SetMousePos(std::nullopt);
		componentMouseClickEvent.reset();
		componentMouseDownEvent.reset();
		componentMouseScrollEvent.reset();
	}

	void View::SetOnTop(bool /* newOnTop */)
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
			HandleMouseLeave();
			HandleWindowHidden();
		}
		if (!onTop && newOnTop)
		{
			HandleWindowShown();
			HandleMouseEnter();
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

	View::ExitEventType View::ClassifyExitEvent(const SDL_Event &event)
	{
		switch (event.type)
		{
		case SDL_KEYDOWN:
			if (!event.key.repeat)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_RETURN:
				case SDLK_RETURN2:
				case SDLK_KP_ENTER:
					return ExitEventType::tryOk;

				case SDLK_ESCAPE:
					return ExitEventType::exit;
				}
			}
			break;
		}
		return ExitEventType::none;
	}
}
