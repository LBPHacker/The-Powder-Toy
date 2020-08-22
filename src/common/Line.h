#pragma once

template<class Func, bool Brush = false>
void Line(int x0, int y0, int x1, int y1, Func func)
{
	// * Octant-agnostic Bresenham code from Wikipedia.
	int dx = x1 - x0;
	int dy = y0 - y1;
	// * With Brush = true, this algorithm connects the otherwise disconnected
	//   line segments. It does this by counting how many times x0 and y0 were
	//   adjusted each iteration; this is either "once" or "twice", "twice"
	//   being the case in which an extra pixel must be drawn.
	// * There are always two spots to choose from to draw the extra pixel at,
	//   and it turns out that in order to match what the legacy brush line
	//   algorithm did, we need to choose based on the slope of the line being
	//   drawn. corr below does this.
	bool corr = dx + dy > 0;
	int sx = 1;
	if (dx < 0)
	{
		sx = -1;
		dx = -dx;
	}
	int sy = 1;
	if (dy > 0)
	{
		sy = -1;
		dy = -dy;
	}
	int err = dx + dy;
	while (true)
	{
		func(x0, y0);
		if (x0 == x1 && y0 == y1)
		{
			break;
		}
		int err2 = 2 * err;
		int changes = 0;
		// * These defaults should never actually be used.
		int px0 = 1;
		int py0 = 1;
		if (err2 >= dy)
		{
			changes += 1;
			py0 = y0;
			px0 = x0;
			err += dy;
			x0 += sx;
		}
		if (err2 <= dx)
		{
			changes += 1;
			py0 = y0;
			px0 = x0;
			err += dx;
			y0 += sy;
		}
		if (Brush && changes == 2)
		{
			if (corr)
			{
				func(px0 - sx, py0 + sy);
			}
			else
			{
				func(px0, py0);
			}
		}
	}
}

template<class Func>
void BrushLine(int x0, int y0, int x1, int y1, Func func)
{
	Line<Func, true>(x0, y0, x1, y1, func);
}
