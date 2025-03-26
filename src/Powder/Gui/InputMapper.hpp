#pragma once
#include "Common/NoCopy.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
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
		struct Action
		{
			std::string name;
			std::function<InputDisposition ()> begin;
			std::function<void ()> end;
			bool active = false;
		};

		struct InputToAction
		{
			Input input;
			std::set<Input> modifiers;
			std::shared_ptr<Action> action;
		};
		struct ActionContext
		{
			std::string name;
			std::shared_ptr<ActionContext> parentContext;
			std::vector<InputToAction> inputToActions;
			std::function<void ()> begin;
			std::function<void ()> end;
			bool active = false;
		};

	private:
		struct ActiveInput
		{
			Input input;
			std::shared_ptr<Action> action;
		};

		std::vector<ActiveInput> activeInputs;
		std::vector<Input> modifiers;
		void InitModifiers();

		std::set<Input> activeModifiers;
		std::shared_ptr<ActionContext> currentActionContext;

		void EndSpecificInputs(decltype(activeInputs)::iterator first);

	public:
		InputMapper();

		ActionContext *GetCurrentActionContext() const
		{
			return currentActionContext.get();
		}
		void SetCurrentActionContext(std::shared_ptr<ActionContext> newActionContext);

		bool HandleEvent(const SDL_Event &event);
		InputDisposition BeginInput(Input input);
		void EndInput(Input input);
		void EndAction(Action *action);
		void EndAllInputs();
	};
}
