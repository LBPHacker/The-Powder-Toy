#include "Main.hpp"
#include "Activity/Game.hpp"
#include "Activity/LocalBrowser.hpp"
#include "Activity/OnlineBrowser.hpp"
#include "Activity/StampBrowser.hpp"
#include "Common/Defer.hpp"
#include "Common/ThreadPool.hpp"
#include "Common/Log.hpp"
#include "common/platform/Platform.h"
#include "Format.h"
#include "Config.h"
#include "client/Client.h"
#include "client/http/requestmanager/RequestManager.h"
#include "Gui/Host.hpp"
#include "Gui/Stack.hpp"
#include "Gui/SdlAssert.hpp"
#include "simulation/SimulationData.h"
#include "prefs/GlobalPrefs.h"
#include <cassert>

namespace Powder
{
	namespace
	{
		constexpr int32_t SCALE_MAXIMUM = 10;

		using Argument = std::optional<ByteString>;
		using Arguments = std::map<ByteString, Argument>;
		Arguments GetArguments(std::span<const ByteString> args)
		{
			Arguments arguments;
			for (int32_t i = 1; i < int32_t(args.size()); ++i)
			{
				auto &str = args[i];
				if (str.BeginsWith("file://"))
				{
					arguments.insert({ "open", format::URLDecode(str.substr(7 /* length of the "file://" prefix */)) });
				}
				else if (str.BeginsWith("ptsave:"))
				{
					arguments.insert({ "ptsave", str });
				}
				else if (auto split = str.SplitBy(':'))
				{
					arguments.insert({ split.Before(), split.After() });
				}
				else if (auto split = str.SplitBy('='))
				{
					arguments.insert({ split.Before(), split.After() });
				}
				else if (str == "open" || str == "ptsave" || str == "ddir")
				{
					if (i + 1 < int32_t(args.size()))
					{
						arguments.insert({ str, args[i + 1] });
						i += 1;
					}
					else
					{
						Log("no value provided for command line parameter ", str);
					}
				}
				else
				{
					arguments.insert({ str, "" }); // so .has_value() is true
				}
			}
			return arguments;
		}

		void HandleDdir(Arguments &arguments)
		{
			if (auto ddirArg = arguments["ddir"])
			{
				if (Platform::ChangeDir(ddirArg.value()))
				{
					Platform::sharedCwd = Platform::GetCwd();
				}
				else
				{
					Log("failed to chdir to requested ddir");
				}
			}
			else if constexpr (SHARED_DATA_FOLDER)
			{
				auto ddir = Platform::DefaultDdir();
				if (!Platform::FileExists("powder.pref"))
				{
					if (!ddir.empty())
					{
						if (!Platform::ChangeDir(ddir))
						{
							Log("failed to chdir to default ddir");
							ddir = {};
						}
					}
				}
				if (!ddir.empty())
				{
					Platform::sharedCwd = ddir;
				}
			}
		}

		bool TrueString(const ByteString &str)
		{
			auto strLower = str.ToLower();
			return strLower == "true" ||
			       strLower == "t" ||
			       strLower == "on" ||
			       strLower == "yes" ||
			       strLower == "y" ||
			       strLower == "1" ||
			       strLower == ""; // standalone "redirect" or "disable-bluescreen" or similar arguments
		}

		bool TrueArg(const Argument &arg)
		{
			return arg && TrueString(arg.value());
		}

		void HandleWindowParameters(Gui::Host &host, Prefs &prefs, Arguments &arguments)
		{
			Gui::WindowParameters windowParameters;
			if (prefs.Get("Fullscreen", false))
			{
				windowParameters.frameType = Gui::WindowParameters::FrameType::fullscreen;
			}
			else if (prefs.Get("Resizable", false))
			{
				windowParameters.frameType = Gui::WindowParameters::FrameType::resizable;
			}
			windowParameters.fullscreenChangeResolution  = prefs.Get("AltFullscreen"      , false);
			windowParameters.fullscreenForceIntegerScale = prefs.Get("ForceIntegerScaling", true );
			windowParameters.fixedScale                  = prefs.Get("Scale"              , 1    );
			windowParameters.blurryScaling               = prefs.Get("BlurryScaling"      , false);
			bool setPrefs = false;
			if (TrueArg(arguments["kiosk"]))
			{
				windowParameters.frameType = Gui::WindowParameters::FrameType::fullscreen;
				setPrefs = true;
			}
			if (auto scaleArg = arguments["scale"])
			{
				try
				{
					windowParameters.fixedScale = scaleArg.value().ToNumber<int32_t>();
					setPrefs = true;
				}
				catch (const std::runtime_error &ex)
				{
					Log("failed to set scale: ", ex.what());
				}
			}
			// TODO: maybe bind the maximum allowed scale to screen size somehow
			if (windowParameters.fixedScale < 1 || windowParameters.fixedScale > SCALE_MAXIMUM)
			{
				Log("invalid scale ", windowParameters.fixedScale, " requested, resetting to 1");
				windowParameters.fixedScale = 1;
				setPrefs = true;
			}
			if (setPrefs)
			{
				windowParameters.SetPrefs();
			}
			host.SetWindowParameters(windowParameters);
			host.SetMomentumScroll(prefs.Get("MomentumScroll", true));
			host.SetShowAvatars   (prefs.Get("ShowAvatars"   , true));
			host.SetFastQuit      (prefs.Get("FastQuit"      , true));
			host.SetGlobalQuit    (prefs.Get("GlobalQuit"    , true));
		}

		void HandleRedirect(Prefs &prefs, Arguments &arguments, Client &client)
		{
			auto redirectStd = prefs.Get("RedirectStd", false);
			if (TrueArg(arguments["console"]))
			{
				Platform::AllocConsole();
			}
			else if (TrueArg(arguments["redirect"]) || redirectStd)
			{
				FILE *new_stdout = freopen("stdout.log", "w", stdout);
				FILE *new_stderr = freopen("stderr.log", "w", stderr);
				if (!new_stdout || !new_stderr)
				{
					Platform::Exit(42);
				}
			}
			client.SetRedirectStd(redirectStd);
		}

		http::RequestManager::Config GetRequestManagerConfig(Prefs &prefs, Arguments &arguments)
		{
			auto clientConfig = [&prefs](Argument arg, ByteString name) {
				if (!arg)
				{
					return prefs.Get<ByteString>(name);
				}
				if (arg->empty())
				{
					arg.reset();
				}
				prefs.Set(name, arg);
				return arg;
			};
			auto proxyString = clientConfig(arguments["proxy"], "Proxy");
			auto cafileString = clientConfig(arguments["cafile"], "CAFile");
			auto capathString = clientConfig(arguments["capath"], "CAPath");
			bool disableNetwork = TrueArg(arguments["disable-network"]);
			return { proxyString, cafileString, capathString, disableNetwork };
		}

		struct Globals
		{
			std::unique_ptr<GlobalPrefs> globalPrefs;
			http::RequestManagerPtr requestManager;
			std::unique_ptr<Client> client;
			std::unique_ptr<SimulationData> simulationData;
			std::unique_ptr<ThreadPool> threadPool;
			std::unique_ptr<Gui::Host> host;
			std::unique_ptr<Stacks> stacks;
		};
		std::unique_ptr<Globals> globals;
	}

	Stacks::Stacks(Gui::Host &newHost) : host(newHost)
	{
	}

	void Stacks::AddStack(std::shared_ptr<Gui::Stack> stack)
	{
		stacks.push_back(stack);
	}

	void Stacks::SelectStack(Gui::Stack *stack)
	{
		for (auto &item : stacks)
		{
			if (item.get() == stack)
			{
				host.SetStack(item);
				return;
			}
		}
		assert(false);
	}

	void MainLoopBody()
	{
		// TODO-REDO_UI-POSTCLEANUP: emscripten_set_main_loop_arg
		globals->client->Tick();
		globals->host->HandleFrame();
	}

	void Main(std::span<const ByteString> args)
	{
		globals = std::make_unique<Globals>();
		Defer resetGlobals([]() {
			globals.reset();
		});
		auto arguments = GetArguments(args);
		HandleDdir(arguments);
		auto &prefs = *(globals->globalPrefs = std::make_unique<GlobalPrefs>());
		globals->requestManager = http::RequestManager::Create(GetRequestManagerConfig(prefs, arguments));
		auto &client = *(globals->client = std::make_unique<Client>());
		client.SetAutoStartupRequest(prefs.Get("AutoStartupRequest", true));
		client.Initialize();
		globals->simulationData = std::make_unique<SimulationData>();
		globals->threadPool = std::make_unique<ThreadPool>();
		auto &host = *(globals->host = std::make_unique<Gui::Host>());
		HandleWindowParameters(host, prefs, arguments);
		HandleRedirect(prefs, arguments, client);

		globals->stacks = std::make_unique<Stacks>(host);

		auto gameStack = std::make_shared<Gui::Stack>(host);
		auto game = std::make_shared<Activity::Game>(host, *globals->threadPool);
		gameStack->Push(game);
		game->SetStacks(globals->stacks.get());
		game->SetGameStack(gameStack.get());

		auto onlineBrowserStack = std::make_shared<Gui::Stack>(host);
		auto onlineBrowser = std::make_shared<Activity::OnlineBrowser>(host, *globals->threadPool);
		onlineBrowserStack->Push(onlineBrowser);
		onlineBrowser->SetStacks(globals->stacks.get());

		auto localBrowserStack = std::make_shared<Gui::Stack>(host);
		auto localBrowser = std::make_shared<Activity::LocalBrowser>(host, *globals->threadPool);
		localBrowserStack->Push(localBrowser);
		localBrowser->SetStacks(globals->stacks.get());

		auto stampBrowserStack = std::make_shared<Gui::Stack>(host);
		auto stampBrowser = std::make_shared<Activity::StampBrowser>(host, *globals->threadPool);
		stampBrowserStack->Push(stampBrowser);
		stampBrowser->SetStacks(globals->stacks.get());

		game->SetOnlineBrowserStack(onlineBrowserStack.get());
		game->SetOnlineBrowser(onlineBrowser.get());
		game->SetLocalBrowserStack(localBrowserStack.get());
		game->SetLocalBrowser(localBrowser.get());
		game->SetStampBrowserStack(stampBrowserStack.get());
		game->SetStampBrowser(stampBrowser.get());
		onlineBrowser->SetGameStack(gameStack.get());
		onlineBrowser->SetGame(game.get());
		localBrowser->SetGameStack(gameStack.get());
		localBrowser->SetGame(game.get());
		stampBrowser->SetGameStack(gameStack.get());
		stampBrowser->SetGame(game.get());

		globals->stacks->AddStack(gameStack);
		globals->stacks->AddStack(onlineBrowserStack);
		globals->stacks->AddStack(localBrowserStack);
		globals->stacks->AddStack(stampBrowserStack);
		globals->stacks->SelectStack(gameStack.get());

		game->Init();
		host.Start();
		while (host.IsRunning())
		{
			MainLoopBody();
		}
	}
}

int main(int argc, char *argv[])
{
	Powder::Main(std::vector<ByteString>(argv, argv + argc));
	return 0;
}
