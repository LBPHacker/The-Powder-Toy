#pragma once
#include <variant>

struct WindowFrameOps
{
	int scale = 1;
	bool resizable = false;
	bool fullscreen = false;
	bool forceIntegerScaling = false;
	bool blurryScaling = false;

	WindowFrameOps Normalize() const
	{
		return {
			fullscreen ? 1     : scale              ,
			fullscreen ? false : resizable          ,
			fullscreen                              ,
			fullscreen ? forceIntegerScaling : false,
			blurryScaling                           ,
		};
	}
};
