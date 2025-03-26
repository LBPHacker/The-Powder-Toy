#pragma once
#include <span>
#include <string_view>
#include <memory>
#include <vector>

namespace Powder::Gui
{
	class Stack;
	class Host;
}

namespace Powder
{
	class Stacks
	{
		Gui::Host &host;

		std::vector<std::shared_ptr<Gui::Stack>> stacks;

	public:
		Stacks(Gui::Host &newHost);
		void AddStack(std::shared_ptr<Gui::Stack> stack);
		void SelectStack(Gui::Stack *stack);
	};

	void Main(std::span<const std::string_view> args);
}
