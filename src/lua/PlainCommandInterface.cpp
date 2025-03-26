#include "CommandInterface.h"

CommandInterfacePtr CommandInterface::Create(Powder::Activity::Game &newGame)
{
	return CommandInterfacePtr(new CommandInterface(newGame));
}

void CommandInterfaceDeleter::operator ()(CommandInterface *ptr) const
{
	delete ptr;
}

void CommandInterface::OnTick()
{
}

void CommandInterface::Init()
{
}

bool CommandInterface::HandleEvent(const GameControllerEvent &event)
{
	return true;
}

bool CommandInterface::HaveSimGraphicsEventHandlers()
{
	return false;
}

int CommandInterface::Command(String command)
{
	return PlainCommand(command);
}

String CommandInterface::FormatCommand(String command)
{
	return PlainFormatCommand(command);
}

void CommandInterface::SetToolIndex(ByteString identifier, std::optional<int> index)
{
}

// void CommandInterface::RemoveComponents() // TODO-REDO_UI
// {
// }
