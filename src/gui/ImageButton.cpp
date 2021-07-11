#include "ImageButton.h"

#include "gui/SDLWindow.h"

namespace gui
{
	void ImageButton::UpdateTexture(bool rendererUp)
	{
		if (texture)
		{
			SDL_DestroyTexture(texture);
			texture = nullptr;
		}
		if (rendererUp && image.size.x && image.size.y)
		{
			texture = SDL_CreateTexture(Renderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, image.size.x, image.size.y);
			if (!texture)
			{
				throw std::runtime_error(ByteString("SDL_CreateTexture failed\n") + SDL_GetError());
			}
			SDL_UpdateTexture(texture, NULL, &image.pixels[0], image.size.x * sizeof(uint32_t));
		}
	}

	void ImageButton::RendererUp()
	{
		UpdateTexture(true);
	}

	void ImageButton::RendererDown()
	{
		UpdateTexture(false);
	}

	void ImageButton::Draw() const
	{
		if (texture)
		{
			auto abs = AbsolutePosition();
			auto size = Size();
			SDL_Rect rect;
			rect.x = abs.x;
			rect.y = abs.y;
			rect.w = size.x;
			rect.h = size.y;
			SDL_RenderCopy(Renderer(), texture, NULL, &rect);
		}
	}

	void ImageButton::ImageData(Image newImage)
	{
		image = newImage;
		UpdateTexture(Renderer());
	}
}
