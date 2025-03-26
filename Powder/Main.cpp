#include "Main.hpp"
#include "Activity/Game.hpp"
#include "Common/Defer.hpp"
#include "Gui/Host.hpp"
#include "Gui/ViewStack.hpp"
#include "simulation/SimulationData.h"
#include "prefs/GlobalPrefs.h"
#include <SDL.h>

namespace Powder
{
	void MainLoopBody()
	{
		Gui::Host::Ref().HandleFrame();
	}

	void Main(const std::vector<std::string_view> &)
	{
		GlobalPrefs globalPrefs;
		SimulationData simulationData;
		Gui::Host host;
		{
			auto viewStack = std::make_shared<Gui::ViewStack>();
			auto game = std::make_shared<Activity::Game>();
			viewStack->Push(game);
			host.SetViewStack(viewStack);
		}
		host.Start();
		while (host.IsRunning())
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
