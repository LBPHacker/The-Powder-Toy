#ifndef COMMANDINTERFACE_H_
#define COMMANDINTERFACE_H_
#include "Config.h"

#include "common/String.h"
#include "lua/LuaEvents.h"

class Event;
class GameModel;
class GameController;
class Tool;

namespace activities
{
namespace game
{
	class Game;
}
}

class CommandInterface
{
protected:
	String lastError;
	activities::game::Game *game;
public:
	enum LogType { LogError, LogWarning, LogNotice };
	enum FormatType { FormatInt, FormatString, FormatChar, FormatFloat, FormatElement };
	CommandInterface(activities::game::Game *game);
	int GetPropertyOffset(ByteString key, FormatType & format);
	void Log(LogType type, String message);
	//void AttachGameModel(GameModel * m);

	virtual void OnTick() { }

	virtual bool HandleEvent(LuaEvents::EventTypes eventType, Event * event) { return true; }

	enum CommandResult
	{
		commandOk,
		commandOkCloseConsole,
		commandNoInterpreter,
		commandWantMore,
		commandSyntaxError,
		commandRuntimeError,
	};
	virtual CommandResult Validate(const String &command);
	virtual CommandResult Execute(const String &command);
	virtual String FormatCommand(const String &command);
	void SetLastError(String err)
	{
		lastError = err;
	}
	String GetLastError();
	virtual ~CommandInterface();
};

#endif /* COMMANDINTERFACE_H_ */
