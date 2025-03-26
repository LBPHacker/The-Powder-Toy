#include "graphics/Renderer.h"
#include "Simulation.h"

template<class Graphics>
void sign::Draw(const RenderableSimulation *sim, Graphics &g) const
{
	int tx, ty, tw, th;
	String text = getDisplayText(sim, tx, ty, tw, th);
	g.DrawFilledRect(RectSized(Vec2{ tx + 1, ty + 1 }, Vec2{ tw, th - 1 }), 0x000000_rgb);
	g.DrawRect(RectSized(Vec2{ tx, ty }, Vec2{ tw+1, th }), 0xC0C0C0_rgb);
	g.BlendText({ tx+3, ty+4 }, text, 0xFFFFFF_rgb .WithAlpha(255));

	if (ju != sign::None)
	{
		int px = x;
		int py = y;
		int dx = 1 - ju;
		int dy = (y > 18) ? -1 : 1;
		for (int j = 0; j < 4; j++)
		{
			g.DrawPixel({ px, py }, 0xC0C0C0_rgb);
			px += dx;
			py += dy;
		}
	}
}
