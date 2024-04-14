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
	      " - \bosimulation size\bg and max particle count, and also pressure and temperature limits \bocan be changed\bg\n"
	      "   in Simulation settings, \bochanges take effect on restart\bg\n"
	      " - invalid simulation parameters are rejected ahead of time so they do not even make it into the configuration\n"
	      " - if invalid parameters do make it into the configuration, they are normalized at startup and a notification\n"
	      "   is shown about the issue\n"
	      " - if for some reason this is not enough, resetting the simulation config to the initial value can be requested with\n"
	      "   the resetsimconfig command line option\n"
	      "\n"
	      "Note that \bosimulations larger than 255 by 255 cells cannot be saved\bg because the save format does not support\n"
	      "such dimensions. Simulation settings warns you about this ahead of time.\n"
	      "\n"
	      "This mod is mostly for demonstrative purposes. Expect bad performance, inability to save, breakage of other sorts,\n"
	      "and little to no support.\n";
	sb << "\n\bt" << VersionInfo();
	return sb.Build();
}
