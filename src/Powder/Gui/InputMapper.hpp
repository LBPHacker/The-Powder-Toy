#pragma once
#include "Common/NoCopy.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

typedef union SDL_Event SDL_Event;

namespace Powder::Gui
{
	class InputMapper : public NoCopy
	{
	public:
		struct MouseButtonInput
		{
			int32_t button;

			auto operator <=>(const MouseButtonInput &) const = default;
		};
		struct MouseWheelInput
		{
			enum class Direction
			{
				positiveX,
				negativeX,
				positiveY,
				negativeY,
			};
			Direction direction;

			auto operator <=>(const MouseWheelInput &) const = default;
		};
		struct KeyboardKeyInput
		{
			int32_t scancode;

			auto operator <=>(const KeyboardKeyInput &) const = default;
		};
		using Input = std::variant<
			MouseButtonInput,
			MouseWheelInput,
			KeyboardKeyInput
		>;

		enum class InputDisposition
		{
			unhandled,
			shared,
			exclusive,
		};
		using ActionKey = std::string;
		struct Action
		{
			std::function<InputDisposition ()> begin;
			std::function<void ()> end;
		};

		using ActionContextKey = std::string;
		struct InputToAction
		{
			Input input;
			std::set<Input> modifiers;
			ActionKey actionKey;
		};
		struct ActionContext
		{
			std::vector<InputToAction> inputToActions;
			std::optional<std::string> parentContext;
		};

	private:
		struct ActiveInput
		{
			Input input;
			ActionKey actionKey;
		};

		std::vector<ActiveInput> activeInputs;
		std::vector<Input> modifiers;
		void InitModifiers();

		struct ActionState
		{
			Action action;
			bool active = false;
		};
		std::set<Input> activeModifiers;
		std::map<ActionKey, ActionState> actions;

		std::map<ActionContextKey, ActionContext> actionContexts;
		std::string currentActionContextKey;

		void EndSpecificInputs(decltype(activeInputs)::iterator first);

	public:
		InputMapper();

		void AddAction(ActionKey key, Action action);
		void RemoveAction(ActionKey key);
		void AddActionContext(ActionContextKey key, ActionContext actionContext);
		void RemoveActionContext(ActionContextKey key);
		void SetCurrentActionContext(ActionContextKey key);

		bool HandleEvent(const SDL_Event &event);
		InputDisposition BeginInput(Input input);
		void EndInput(Input input);
		void EndAllInputs();
	};
}
