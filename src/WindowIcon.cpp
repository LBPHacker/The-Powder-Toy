#include "Format.h"
#include "Common/Defer.hpp"
#include "Powder/Gui/SdlAssert.hpp"
#include "graphics/VideoBuffer.h"
#include "WindowIcon.h"
#include "icon_exe_png.h"

void WindowIcon(SDL_Window *window)
{
	if (auto image = format::PixelsFromPNG(icon_exe_png.AsCharSpan()))
	{
		auto *icon = SdlAssertPtr(SDL_CreateRGBSurfaceFrom(image->data(), image->Size().X, image->Size().Y, 32, image->Size().Y * sizeof(pixel), 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000));
		Powder::Defer destroyIcon([icon]() {
			SDL_FreeSurface(icon);
		});
		SDL_SetWindowIcon(window, icon);
	}
}
