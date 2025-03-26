#include "InputMapper.hpp"
#include "Icons.hpp"
#include "SdlAssert.hpp"
#include <algorithm>
#include <deque>
#include <sstream>

namespace Powder::Gui
{
	namespace
	{
		template<class Set>
		bool IsSubsetOf(const Set &subject, const Set &other)
		{
			auto subjectIt = subject.begin();
			auto otherIt = other.begin();
			while (subjectIt != subject.end())
			{
				if (otherIt == other.end() || *otherIt > *subjectIt)
				{
					return false;
				}
				if (*otherIt == *subjectIt)
				{
					++subjectIt;
				}
				++otherIt;
			}
			return true;
		}
	}

	InputMapper::InputMapper()
	{
		InitKnownModifiers();
	}

	InputMapper::InputDisposition InputMapper::BeginInput(Input input)
	{
		auto foundModifier = std::find(knownModifiers.begin(), knownModifiers.end(), input);
		auto inputIsModifier = foundModifier != knownModifiers.end();
		if (inputIsModifier)
		{
			activeModifiers.insert(input);
		}
		auto foundActiveInput = std::find_if(activeInputs.begin(), activeInputs.end(), [input](auto &activeInput) {
			if (auto *realInput = std::get_if<Input>(&activeInput.input))
			{
				return *realInput == input;
			}
			return false;
		});
		if (foundActiveInput != activeInputs.end())
		{
			return InputDisposition::unhandled;
		}
		auto actionContext = currentActionContext;
		auto cumulativeDisposition = InputDisposition::unhandled;
		auto requiredModifiers = activeModifiers;
		while (actionContext)
		{
			for (auto &inputToAction : actionContext->inputToActions)
			{
				if (auto *realInput = std::get_if<Input>(&inputToAction.input))
				{
					bool matches = false;
					if (*realInput == input)
					{
						auto *requiredModifiers = &activeModifiers;
						std::optional<std::set<Input>> reducedModifiers;
						for (auto &activeInput : activeInputs)
						{
							auto &modifierFor = activeInput.action->modifierFor;
							if (std::find(modifierFor.begin(), modifierFor.end(), inputToAction.action) != modifierFor.end())
							{
								if (!reducedModifiers)
								{
									reducedModifiers = activeModifiers;
									requiredModifiers = &*reducedModifiers;
								}
								for (auto &modifier : activeInput.modifiers)
								{
									requiredModifiers->erase(modifier);
								}
							}
						}
						matches = inputToAction.modifiers == *requiredModifiers;
					}
					if (!matches)
					{
						continue;
					}
				}
				else
				{
					if (!IsSubsetOf(inputToAction.modifiers, activeModifiers))
					{
						continue;
					}
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
					activeInputs.push_back({ inputToAction.input, inputToAction.modifiers, action });
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

	void InputMapper::EndAllModifiers()
	{
		while (!activeModifiers.empty())
		{
			EndInput(*activeModifiers.begin());
		}
	}

	void InputMapper::EndAllInputs()
	{
		// End modifiers first so no ModifierOnly input stays active for the next loop, which can't deal with those.
		EndAllModifiers();
		while (!activeInputs.empty())
		{
			if (auto *realInput = std::get_if<Input>(&activeInputs.back().input))
			{
				EndInput(*realInput);
			}
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
		auto foundModifier = std::find(knownModifiers.begin(), knownModifiers.end(), input);
		if (foundModifier != knownModifiers.end())
		{
			activeModifiers.erase(input);
		}
		EndSpecificInputs(std::partition(activeInputs.begin(), activeInputs.end(), [input](auto &activeInput) {
			if (auto *realInput = std::get_if<Input>(&activeInput.input))
			{
				if (*realInput == input)
				{
					return false;
				}
			}
			if (activeInput.modifiers.find(input) != activeInput.modifiers.end())
			{
				return false;
			}
			return true;
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

	void InputMapper::InitKnownModifiers()
	{
		// TODO-REDO_UI: interpret right-side modifiers
		knownModifiers.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LCTRL } });
		knownModifiers.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LSHIFT } });
		knownModifiers.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LALT } });
	}

	void InputMapper::SetCurrentActionContext(std::shared_ptr<ActionContext> newActionContext)
	{
		EndAllModifiers();
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
