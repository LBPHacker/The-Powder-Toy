#pragma once
#include "Config.h"
#include "SimulationConfig.h"
#include "common/String.h"

inline ByteString VersionInfo()
{
	ByteStringBuilder sb;
	sb << DISPLAY_VERSION[0] << "." << DISPLAY_VERSION[1];
	if constexpr (!SNAPSHOT)
	{
		sb << "." << APP_VERSION.build;
	}
	sb << " " << IDENT;
	if constexpr (MOD)
	{
		sb << " MOD " << MOD_ID << " UPSTREAM " << UPSTREAM_VERSION.build;
	}
	if constexpr (SNAPSHOT)
	{
		sb << " SNAPSHOT " << APP_VERSION.build;
	}
	if constexpr (LUACONSOLE)
	{
		sb << " LUACONSOLE";
	}
	if constexpr (NOHTTP)
	{
		sb << " NOHTTP";
	}
	else if constexpr (ENFORCE_HTTPS)
	{
		sb << " HTTPS";
	}
	if constexpr (DEBUG)
	{
		sb << " DEBUG";
	}
	return sb.Build();
}

inline ByteString IntroText()
{
	ByteStringBuilder sb;
	sb << "\bl\bU" << APPNAME << "\bU - Version " << DISPLAY_VERSION[0] << "." << DISPLAY_VERSION[1] << " - https://powdertoy.co.uk, irc.libera.chat #powder, https://tpt.io/discord\n"
	      "\bg\n"
	      "Press \bo'F1'\bg to show or hide this text. New features compared to vanilla:\n"
	      "\n"
	      " - \bomultithreaded simulation\bg, with caveats: not all of the simulation is multithreaded, and not always,\n"
	      "   but it still can provide very noticeable performance boosts\n"
	      " - new command line arg: \bothreads:N\bg\n"
	      " - new Lua function: \bosim.threads(N)\bg\n"
	      " - a few new HUD readouts about timing in the top left area if you enable \botpt.debug(tpt.DEBUG_FRAMETIME)\bg.\n"
	      "\n"
	      "You \boWILL have to use sim.threads(N) to get any multithreading action\bg because the initial value is 0, e.g.\n"
	      "try sim.threads(4). To experience actual speedup, you may also have to uncap FPS with \botpt.fpsCap(2)\bg.\n"
	      "You can try using more and more threads until you hit a point where FPS doesn't go any higher. Do not\n"
	      "expect linear speedup, e.g. 8x FPS for 8 threads, but definitely expect significant speedup.\n"
	      "\n"
	      "sim.threads(0) is \bocompatible with the old simulation\bg, so everything works as it used to. sim.threads(N)\n"
	      "for N >= 1 \bomay break things\bg, see below. sim.threads(1) is useful for establishing a baseline to compare\n"
	      "higher N against, because it runs the same multi-threading-capable code, unlike sim.threads(0).\n"
	      "\n"
	      "Known issues (will be updated as more issues are discovered or fixed):\n"
	      "\n"
	      " - when using multiple threads, \bosubframe is broken\bg; this is by design and will not be fixed\n"
	      " - when using multiple threads, partial frame steps hinder multithreading and are no longer a useful debug tool\n"
	      " - water equalization and \bosome Lua scripts hinder multithreading\bg; the game will behave as if you specified\n"
	      "   sim.threads(1)\n";
	sb << "\n\bt" << VersionInfo();
	return sb.Build();
}
