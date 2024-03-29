#include "Format.h"
#include "graphics/VideoBuffer.h"
#include "WindowIcon.h"

#include "icon_exe.png.h"

void WindowIcon(SDL_Window *window)
{
	if (auto image = format::PixelsFromPNG(std::vector<char>(icon_exe_png, icon_exe_png + icon_exe_png_size)))
	{
		SDL_Surface *icon = SDL_CreateSurfaceFrom(image->data(), image->Size().X, image->Size().Y, image->Size().Y * sizeof(pixel), SDL_PIXELFORMAT_ARGB8888);
		SDL_SetWindowIcon(window, icon);
		SDL_DestroySurface(icon);
	}
}
