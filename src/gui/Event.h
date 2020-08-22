#pragma once

#include "Point.h"

class String;
class ByteString;
struct SDL_Renderer;

namespace gui
{
	struct Event
	{
		enum Type
		{
			TICK,
			DRAW,
			QUIT,
			FOCUSGAIN,
			FOCUSLOSE,
			MOUSEENTER,
			MOUSELEAVE,
			MOUSEMOVE,
			MOUSEDOWN,
			MOUSEUP,
			MOUSEWHEEL,
			KEYPRESS,
			KEYRELEASE,
			TEXTINPUT,
			TEXTEDITING,
			FILEDROP,
			RENDERERUP,
			RENDERERDOWN,
		} type;
		union
		{
			struct
			{
				Point current;
				int button, wheel, clicks;
			} mouse;
			struct
			{
				int sym, scan;
				bool repeat, shift, ctrl, alt;
			} key;
			struct
			{
				const String *data;
			} text;
			struct
			{
				const String *path;
			} filedrop;
			struct
			{
				SDL_Renderer *renderer;
			} renderer;
		};
	};
}
