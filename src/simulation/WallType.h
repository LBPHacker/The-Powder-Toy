#ifndef WALLTYPE_H_
#define WALLTYPE_H_

#include "common/String.h"
#include "graphics/Pixel.h"
class VideoBuffer;

struct wall_type
{
	pixel colour;
	pixel eglow; // if emap set, add this to fire glow
	enum DrawStyle
	{
		SOLIDDOT,
		SPARSEDOT,
		HEXAGONAL,
		POWERED0,
		POWERED1,
		DIAGONAL,
		SPECIAL,
	};
	DrawStyle drawstyle;
	VideoBuffer * (*textureGen)(int, int, int);
	String name;
	ByteString identifier;
	String descs;
};

#endif
