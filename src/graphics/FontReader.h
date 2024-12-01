#pragma once
#include <cstddef>

#include "common/String.h"

constexpr auto FONT_H = 12;

class FontReader
{
	const unsigned char *pointer;
	int width;
	int pixels;
	int data;

	FontReader(const unsigned char *_pointer);
	static const unsigned char *lookupChar(String::value_type ch);

public:
	FontReader(String::value_type ch);
	int GetWidth() const;
	int NextPixel();
};
