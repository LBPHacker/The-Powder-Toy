#include "InputMapper.hpp"
#include "SdlAssert.hpp"
#include <algorithm>
#include <deque>

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
		auto actionContext = currentActionContext;
		auto cumulativeDisposition = InputDisposition::unhandled;
		while (actionContext)
		{
			for (auto &inputToAction : actionContext->inputToActions)
			{
				// If the input itself is a modifier, don't check modifier state.
				auto inputMatches = inputToAction.input == input;
				auto modifiersMatch = inputToAction.modifiers == activeModifiers;
				if (!(inputMatches && (inputIsModifier || modifiersMatch)))
				{
					continue;
				}
				auto action = inputToAction.action;
				if (action->active)
				{
					continue;
				}
				auto disposition = InputDisposition::exclusive;
				if (action->begin)
				{
					disposition = action->begin();
				}
				cumulativeDisposition = std::max(cumulativeDisposition, disposition);
				if (disposition != InputDisposition::unhandled)
				{
					activeInputs.push_back({ input, action });
					action->active = true;
					if (disposition == InputDisposition::exclusive)
					{
						return cumulativeDisposition;
					}
				}
			}
			actionContext = actionContext->parentContext;
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

	void InputMapper::EndAction(Action *action)
	{
		if (action->end)
		{
			action->end();
		}
		action->active = false;
	}

	void InputMapper::EndSpecificInputs(decltype(activeInputs)::iterator first)
	{
		for (auto it = first; it != activeInputs.end(); ++it)
		{
			EndAction(it->action.get());
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

	void InputMapper::SetCurrentActionContext(std::shared_ptr<ActionContext> newActionContext)
	{
		using Path = std::deque<std::shared_ptr<ActionContext>>;
		auto getForwardPathFromCurrent = [this, newActionContext]() -> std::optional<Path> {
			Path forwardPath;
			auto context = newActionContext;
			while (true)
			{
				if (context == currentActionContext)
				{
					return forwardPath;
				}
				if (!context)
				{
					break;
				}
				forwardPath.push_front(context);
				context = context->parentContext;
			}
			return std::nullopt;
		};
		std::optional<Path> forwardPath;
		while (true)
		{
			forwardPath = getForwardPathFromCurrent();
			if (forwardPath)
			{
				break;
			}
			if (currentActionContext->end)
			{
				currentActionContext->end();
			}
			currentActionContext->active = false;
			currentActionContext = currentActionContext->parentContext;
		}
		if (!forwardPath)
		{
			return;
		}
		for (auto &context : *forwardPath)
		{
			currentActionContext = context;
			currentActionContext->active = true;
			if (context->begin)
			{
				context->begin();
			}
		}
	}
}
