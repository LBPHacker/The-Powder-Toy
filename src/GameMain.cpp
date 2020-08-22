#include "gui/SDLWindow.h"
#include "activities/game/Game.h"

#include "client/Client.h"

#include <stdexcept>
#include <iostream>
#include <map>
#include <csignal>
#include <cstdlib>
#ifdef X86_SSE
# include <xmmintrin.h>
#endif
#ifdef X86_SSE3
# include <pmmintrin.h>
#endif
#ifdef WIN
# include <direct.h>
#else
# include <unistd.h>
#endif
#ifdef MACOSX
# include <CoreServices/CoreServices.h>
#endif
#include <sys/stat.h>

#if !defined(DEBUG) && !defined(_DEBUG)
# define ENABLE_BLUESCREEN
#endif

using ArgumentMap = std::map<ByteString, ByteString>;

static ArgumentMap readArguments(int argc, char *argv[])
{
	std::map<ByteString, ByteString> arguments;
	arguments["scale"] = "";
	arguments["proxy"] = "";
	arguments["kiosk"] = "";
	arguments["redirect"] = "";
	arguments["open"] = "";
	arguments["ddir"] = "";
	arguments["ptsave"] = "";
	arguments["disable-network"] = "";
	for (int i = 1; i < argc; ++i)
	{
		ByteString arg(argv[i]);
		auto split = arg.SplitBy(':');
		auto key = split ? split.Before() : arg;
		auto value = split ? split.After() : ByteString("true");
		auto it = arguments.find(key);
		if (it != arguments.end())
		{
			if (key == "open" || key == "ddir" || key == "ptsave")
			{
				if (i + 1 < argc)
				{
					it->second = argv[++i];
				}
				else
				{
					std::cerr << "argument #" << i << " ignored: no value provided" << std::endl;
				}
			}
			else
			{
				it->second = value;
			}
		}
		else
		{
			std::cerr << "argument #" << i << " ignored: unrecognized option" << std::endl;
		}
	}
	return arguments;
}

static void doDdir(const ArgumentMap &arguments)
{
	if (arguments.at("ddir").length())
	{
#ifdef WIN
		int failure = _chdir(arguments.at("ddir").c_str());
#else
		int failure = chdir(arguments.at("ddir").c_str());
#endif
		if (failure)
		{
			perror("failed to chdir to requested ddir");
		}
	}
	else
	{
#ifdef WIN
		struct _stat s;
		if (_stat("powder.pref", &s))
#else
		struct stat s;
		if (stat("powder.pref", &s))
#endif
		{
			auto ddir = gui::SDLWindow::GetPrefPath();
			if (ddir.size())
			{
#ifdef WIN
				int failure = _chdir(ddir.c_str());
#else
				int failure = chdir(ddir.c_str());
#endif
				if (failure)
				{
					perror("failed to chdir to default ddir");
				}
			}
		}
	}
}

static void doRedirect(const ArgumentMap &arguments)
{
	if (arguments.at("redirect") == "true")
	{
		FILE *newStdout = freopen("stdout.log", "w", stdout);
		FILE *newStderr = freopen("stderr.log", "w", stderr);
		if (!newStdout || !newStderr)
		{
			// * No point in printing an error to stdout/stderr since the user
			//   probably requests those streams be redirected because they
			//   can't see them otherwise. So just throw an exception instead
			//   and hope that the OS and the standard library is smart enough
			//   to display the error message in some useful manner.
			throw std::runtime_error("cannot honour request to redirect standard streams to log files");
		}
	}
}

static void doScale(const ArgumentMap &arguments, int &scale)
{
	if (arguments.at("scale").length())
	{
		try
		{
			scale = arguments.at("scale").ToNumber<int>();
		}
		catch (std::runtime_error &)
		{
		}
		Client::Ref().SetPref("Scale", scale);
	}
}

static void doKiosk(const ArgumentMap &arguments, bool &fullscreen)
{
	if (arguments.at("kiosk").length())
	{
		fullscreen = arguments.at("kiosk") == "true";
		Client::Ref().SetPref("Fullscreen", fullscreen);
	}
}

static void doProxy(const ArgumentMap &arguments, ByteString &proxy)
{
	if (arguments.at("proxy").length())
	{
		proxy = arguments.at("proxy");
		if (proxy == "false")
		{
			proxy = "";
		}
		Client::Ref().SetPref("Proxy", proxy);
	}
}

static void doDisableNetwork(const ArgumentMap &arguments, bool &disableNetwork)
{
	if (arguments.at("disable-network").length())
	{
		disableNetwork = arguments.at("disable-network") == "true";
	}
}

#ifdef ENABLE_BLUESCREEN
static void sigHandler(int sig)
{
	switch (sig)
	{
	case SIGSEGV:
		gui::SDLWindow::Ref().BlueScreen("Memory read/write error");
		break;

	case SIGFPE:
		gui::SDLWindow::Ref().BlueScreen("Floating point exception");
		break;

	case SIGILL:
		gui::SDLWindow::Ref().BlueScreen("Invalid instruction");
		break;

	case SIGABRT:
		gui::SDLWindow::Ref().BlueScreen("Unexpected program abort");
		break;
	}
}
#endif

static void frame()
{
	auto &sdlw = gui::SDLWindow::Ref();
	sdlw.FrameBegin();
	sdlw.FramePoll();
	if (!sdlw.Running())
	{
		return;
	}
	Client::Ref().Tick();
	sdlw.FrameTick();
	sdlw.FrameDraw();
	sdlw.FrameEnd();
	sdlw.FrameDelay();
	sdlw.FrameTime();
}

#ifdef main
# undef main // thank you sdl
#endif

int main(int argc, char *argv[])
{
#if defined(_DEBUG) && defined(_MSC_VER)
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif

#ifdef ENABLE_BLUESCREEN
	//Get ready to catch any dodgy errors
	signal(SIGSEGV, sigHandler);
	signal(SIGFPE, sigHandler);
	signal(SIGILL, sigHandler);
	signal(SIGABRT, sigHandler);
#endif

#ifdef X86_SSE
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
#ifdef X86_SSE3
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

	ArgumentMap arguments = readArguments(argc, argv);
	doRedirect(arguments);
	doDdir(arguments);

	Client cli; // * Needs to happen after doDdir.

	int scale = cli.GetPrefInteger("Scale", 1);
	bool fullscreen = cli.GetPrefBool("Fullscreen", false);
	bool borderless = cli.GetPrefBool("AltFullscreen", false);
	bool resizable = cli.GetPrefBool("Resizable", false);
	bool integer = cli.GetPrefBool("ForceIntegerScaling", true);
	doScale(arguments, scale);
	doKiosk(arguments, fullscreen);

	// * TODO-REDO_UI: replicate guess best scale behaviour
	// * TODO-REDO_UI: replicate open behaviour
	// * TODO-REDO_UI: replicate ptsave behaviour
	// * TODO-REDO_UI: replicate window position behaviour
	// * TODO-REDO_UI: replicate draw limit behaviour

	ByteString proxy = cli.GetPrefByteString("Proxy", "");
	bool disableNetwork = false;
	doProxy(arguments, proxy);
	doDisableNetwork(arguments, disableNetwork);
	cli.Initialise(proxy, disableNetwork);
	
	{
		gui::SDLWindow sdlw(gui::SDLWindow::Configuration{ gui::Point{ WINDOWW, WINDOWH }, scale, fullscreen, borderless, resizable, integer });
		sdlw.MomentumScroll(Client::Ref().GetPrefBool("MomentumScroll", true));
		sdlw.FastQuit(Client::Ref().GetPrefBool("FastQuit", true));

#ifdef ENABLE_BLUESCREEN
		try
		{
#endif
			auto g = sdlw.EmplaceBack<activities::game::Game>();
			g->Test();
			while (sdlw.Running())
			{
				frame();
			}
#ifdef ENABLE_BLUESCREEN
		}
		catch (const std::exception &ex)
		{
			gui::SDLWindow::Ref().BlueScreen(ByteString(ex.what()).FromUtf8());
		}
#endif
	}

	return 0;
}
