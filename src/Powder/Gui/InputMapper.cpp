#include "InputMapper.hpp"
#include "SdlAssert.hpp"
#include <algorithm>

namespace Powder::Gui
{
	InputMapper::InputMapper()
	{
		InitModifiers();
	}

	InputMapper::InputDisposition InputMapper::BeginInput(Input input)
	{
		auto foundModifier = std::find(modifiers.begin(), modifiers.end(), input);
		auto inputIsModifier = foundModifier != modifiers.end();
		if (inputIsModifier)
		{
			activeModifiers.insert(input);
		}
		auto foundActiveInput = std::find_if(activeInputs.begin(), activeInputs.end(), [input](auto &activeInput) {
			return activeInput.input == input;
		});
		if (foundActiveInput != activeInputs.end())
		{
			return InputDisposition::unhandled;
		}
		std::optional<std::string> actionContextKey = currentActionContextKey;
		auto cumulativeDisposition = InputDisposition::unhandled;
		while (actionContextKey)
		{
			auto actionContextIt = actionContexts.find(*actionContextKey);
			if (actionContextIt == actionContexts.end())
			{
				break;
			}
			auto &actionContext = actionContextIt->second;
			for (auto &inputToAction : actionContext.inputToActions)
			{
				// If the input itself is a modifier, don't check modifier state.
				auto inputMatches = inputToAction.input == input;
				auto modifiersMatch = inputToAction.modifiers == activeModifiers;
				if (!(inputMatches && (inputIsModifier || modifiersMatch)))
				{
					continue;
				}
				auto it = actions.find(inputToAction.actionKey);
				if (it == actions.end())
				{
					continue;
				}
				auto &actionState = it->second;
				if (actionState.active)
				{
					continue;
				}
				auto disposition = InputDisposition::exclusive;
				if (actionState.action.begin)
				{
					disposition = actionState.action.begin();
				}
				cumulativeDisposition = std::max(cumulativeDisposition, disposition);
				if (disposition != InputDisposition::unhandled)
				{
					activeInputs.push_back({ input, inputToAction.actionKey });
					actionState.active = true;
					if (disposition == InputDisposition::exclusive)
					{
						return cumulativeDisposition;
					}
				}
			}
			actionContextKey = actionContext.parentContext;
		}
		return cumulativeDisposition;
	}

	void InputMapper::EndAllInputs()
	{
		while (!activeInputs.empty())
		{
			EndInput(activeInputs.back().input);
		}
		while (!activeModifiers.empty())
		{
			EndInput(*activeModifiers.begin());
		}
	}

	void InputMapper::EndSpecificInputs(decltype(activeInputs)::iterator first)
	{
		for (auto it = first; it != activeInputs.end(); ++it)
		{
			auto &actionState = actions.find(it->actionKey)->second;
			if (actionState.action.end)
			{
				actionState.action.end();
			}
			actionState.active = false;
		}
		activeInputs.erase(first, activeInputs.end());
	}

	void InputMapper::EndInput(Input input)
	{
		auto foundModifier = std::find(modifiers.begin(), modifiers.end(), input);
		if (foundModifier != modifiers.end())
		{
			activeModifiers.erase(input);
		}
		EndSpecificInputs(std::partition(activeInputs.begin(), activeInputs.end(), [input](auto &activeInput) {
			return activeInput.input != input;
		}));
	}

	bool InputMapper::HandleEvent(const SDL_Event &event)
	{
		switch (event.type)
		{
		case SDL_MOUSEBUTTONDOWN:
			if (BeginInput(MouseButtonInput{ event.button.button }) == InputDisposition::exclusive)
			{
				return true;
			}
			break;

		case SDL_MOUSEBUTTONUP:
			EndInput(MouseButtonInput{ event.button.button });
			break;

		case SDL_MOUSEWHEEL:
			{
				auto cumulativeDisposition = InputDisposition::unhandled;
				auto translateToInput = [this, &cumulativeDisposition](Input input, int32_t amount) {
					for (int32_t i = 0; i < amount; ++i)
					{
						auto disposition = BeginInput(input);
						cumulativeDisposition = std::max(cumulativeDisposition, disposition);
						EndInput(input);
					}
				};
				int32_t sign = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
				int32_t dx = event.wheel.x * sign;
				int32_t dy = event.wheel.y * sign;
				translateToInput(MouseWheelInput{ MouseWheelInput::Direction::positiveX },  dx);
				translateToInput(MouseWheelInput{ MouseWheelInput::Direction::negativeX }, -dx);
				translateToInput(MouseWheelInput{ MouseWheelInput::Direction::positiveY },  dy);
				translateToInput(MouseWheelInput{ MouseWheelInput::Direction::negativeY }, -dy);
				if (cumulativeDisposition == InputDisposition::exclusive)
				{
					return true;
				}
			}
			break;

		case SDL_KEYDOWN:
			if (event.type == SDL_KEYDOWN && event.key.repeat)
			{
				break;
			}
			if (BeginInput(KeyboardKeyInput{ event.key.keysym.scancode }) == InputDisposition::exclusive)
			{
				return true;
			}
			break;

		case SDL_KEYUP:
			EndInput(KeyboardKeyInput{ event.key.keysym.scancode });
			break;
		}
		return false;
	}

	void InputMapper::InitModifiers()
	{
		// TODO-REDO_UI: interpret right-side modifiers
		modifiers.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LCTRL } });
		modifiers.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LSHIFT } });
		modifiers.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LALT } });
	}

	void InputMapper::AddAction(ActionKey key, Action action)
	{
		RemoveAction(key);
		actions.insert({ std::move(key), ActionState{ std::move(action), false } });
	}

	void InputMapper::RemoveAction(ActionKey key)
	{
		auto it = actions.find(key);
		if (it == actions.end())
		{
			return;
		}
		EndSpecificInputs(std::partition(activeInputs.begin(), activeInputs.end(), [key](auto &activeInput) {
			return activeInput.actionKey != key;
		}));
		actions.erase(it);
	}

	void InputMapper::AddActionContext(ActionContextKey key, ActionContext actionContext)
	{
		actionContexts.insert({ std::move(key), std::move(actionContext) });
	}

	void InputMapper::RemoveActionContext(ActionContextKey key)
	{
		auto it = actionContexts.find(key);
		if (it == actionContexts.end())
		{
			return;
		}
		actionContexts.erase(it);
	}

	void InputMapper::SetCurrentActionContext(ActionContextKey key)
	{
		currentActionContextKey = key;
	}
}
