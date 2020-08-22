#pragma once

#define FONT_H 12

namespace gui
{
	struct FontData
	{
		unsigned char *bits = nullptr;
		unsigned int *ptrs = nullptr;
		unsigned int (*ranges)[2] = nullptr;
	};

	const FontData &GetFontData();
}
