#pragma once

#include "gui/Button.h"
#include "gui/Image.h"

#include <vector>

namespace gui
{
	class ImageButton : public gui::Button
	{
		SDL_Texture *texture = nullptr;

		void UpdateTexture(bool rendererUp);
		Image image;

	public:
		void Draw() const final override;
		void RendererUp() final override;
		void RendererDown() final override;

		void ImageData(Image newImage);
		const Image &ImageData() const
		{
			return image;
		}
	};
}
