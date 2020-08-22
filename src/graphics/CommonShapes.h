#pragma once

#include "Config.h"
#include "Pix.h"
#include "simulation/SimulationData.h"

template<class DrawPixel>
void StreamBase(int x, int y, DrawPixel drawPixel)
{
	for (int j = -2; j <= 2; ++j)
	{
		for (int i = -2; i <= 2; ++i)
		{
			int br = 255 - (i * i + j * j) * 28;
			drawPixel(x + i, y + j, PixRGBA(br, br, br, 128));
		}
	}
}

template<class DrawPixel>
void RedCross(int x, int y, DrawPixel drawPixel)
{
	for (auto i = 0; i < 8; ++i)
	{
		drawPixel(x     + i, y + i, 0xFFFF0000);
		drawPixel(x + 1 + i, y + i, 0xFFFF0000);
		drawPixel(x + 8 - i, y + i, 0xFFFF0000);
		drawPixel(x + 7 - i, y + i, 0xFFFF0000);
	}
}

template<class DrawPixel>
void Wall(int x, int y, bool powered, int drawstyle, uint32_t pc, uint32_t gc, DrawPixel drawPixel)
{
	switch (drawstyle)
	{
	case wall_type::SPECIAL:
		break;

	case wall_type::POWERED0:
	case wall_type::POWERED1:
		for (int j = 0; j < CELL; ++j)
		{
			for (int i = 0; i < CELL; ++i)
			{
				if ((i & j & 1) ^ powered ^ (drawstyle == wall_type::POWERED1))
				{
					drawPixel(x * CELL + i, y * CELL + j, pc);
				}
			}
		}
		break;

	case wall_type::HEXAGONAL:
		for (int j = 0; j < CELL; j += 2)
		{
			for (int i = j >> 1; i < CELL; i += 2)
			{
				drawPixel(x * CELL + i, y * CELL + j, pc);
			}
		}
		break;

	case wall_type::SOLIDDOT:
	case wall_type::SPARSEDOT:
		for (int j = 0; j < CELL; ++j)
		{
			for (int i = 0; i < CELL; ++i)
			{
				if ((i | j) & 1)
				{
					if (drawstyle == wall_type::SOLIDDOT)
					{
						drawPixel(x * CELL + i, y * CELL + j, 0x808080);
					}
				}
				else
				{
					drawPixel(x * CELL + i, y * CELL + j, pc);
				}
			}
		}
		break;

	case wall_type::DIAGONAL:
		for (int j = 0; j < CELL; ++j)
		{
			for (int i = 0; i < CELL; ++i)
			{
				switch ((i - j) & 3)
				{
				case 0:
					drawPixel(x * CELL + i, y * CELL + j, pc);
					break;

				case 1:
					drawPixel(x * CELL + i, y * CELL + j, gc);
					break;
					
				case 2:
				case 3:
					drawPixel(x * CELL + i, y * CELL + j, 0x202020);
					break;
				}
			}
		}
		break;
	}
}
