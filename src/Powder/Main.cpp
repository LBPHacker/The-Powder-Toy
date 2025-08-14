#include "Main.hpp"
#include "Activity/Game.hpp"
#include "Activity/OnlineBrowser.hpp"
#include "Common/Defer.hpp"
#include "client/Client.h"
#include "client/http/requestmanager/RequestManager.h"
#include "Gui/Host.hpp"
#include "Gui/Stack.hpp"
#include "Gui/SdlAssert.hpp"
#include "simulation/SimulationData.h"
#include "prefs/GlobalPrefs.h"

namespace Powder
{
	namespace
	{
		struct Globals
		{
			std::unique_ptr<GlobalPrefs> globalPrefs;
			http::RequestManagerPtr requestManager;
			std::unique_ptr<Client> client;
			// std::unique_ptr<SaveRenderer> saveRenderer;
			std::unique_ptr<SimulationData> simulationData;
			std::unique_ptr<Gui::Host> host;
			std::shared_ptr<Gui::Stack> gameStack;
			std::shared_ptr<Gui::Stack> browserStack;
		};
		std::unique_ptr<Globals> globals;
	}

	void MainLoopBody()
	{
		// TODO-REDO_UI: emscripten_set_main_loop_arg
		globals->client->Tick();
		globals->host->HandleFrame();
	}

	void Main(std::span<const std::string_view>)
	{
		globals = std::make_unique<Globals>();
		Defer resetGlobals([]() {
			globals.reset();
		});
		globals->globalPrefs = std::make_unique<GlobalPrefs>();
		globals->requestManager = http::RequestManager::Create({ std::nullopt, std::nullopt, std::nullopt, false });
		globals->client = std::make_unique<Client>();
		globals->client->SetAutoStartupRequest(true);
		globals->client->Initialize();
		globals->client->SetRedirectStd(false);
		globals->simulationData = std::make_unique<SimulationData>();
		globals->host = std::make_unique<Gui::Host>();
		globals->gameStack = std::make_shared<Gui::Stack>(*globals->host);
		globals->browserStack = std::make_shared<Gui::Stack>(*globals->host);
		auto game = std::make_shared<Activity::Game>(*globals->host);
		globals->gameStack->Push(game);
		auto onlineBrowser = std::make_shared<Activity::OnlineBrowser>(*globals->host);
		globals->browserStack->Push(onlineBrowser);
		onlineBrowser->SetGameStack(globals->gameStack);
		game->SetOnlineBrowserStack(globals->browserStack);
		onlineBrowser->SetGame(game.get());
		game->SetBrowser(onlineBrowser.get());
		globals->host->SetStack(globals->gameStack);
		globals->host->Start();
		while (globals->host->IsRunning())
		{
			MainLoopBody();
		}
	}
}

int main(int argc, char *argv[])
{
	Powder::Main(std::vector<std::string_view>(argv, argv + argc));
	return 0;
}
