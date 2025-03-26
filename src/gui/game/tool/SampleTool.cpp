#include "SampleTool.h"
#include "PropertyTool.h"
#include "GOLTool.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "Activity/Game.hpp"
#include "gui/game/GameView.h"
#include "gui/interface/Colour.h"
#include "simulation/Simulation.h"
#include "simulation/ElementClasses.h"
#include "gui/game/Menu.h"

std::unique_ptr<VideoBuffer> SampleTool::GetIcon(int toolID, Vec2<int> size)
{
	auto texture = std::make_unique<VideoBuffer>(size);
	texture->DrawRect(size.OriginRect(), 0xA0A0A0_rgb);
	texture->BlendChar((size / 2) - Vec2(5, 5), 0xE066, 0xFFFFFF_rgb .WithAlpha(0xFF));
	texture->BlendChar((size / 2) - Vec2(5, 5), 0xE06B, 0x64B4FF_rgb .WithAlpha(0xFF));
	return texture;
}

void SampleTool::Draw(Simulation * sim, Brush const &brush, ui::Point position)
{
	if(game.GetShowingDecoTools())
	{
		if (auto colour = game.GetColorUnderMouse())
		{
			game.SetDecoColor(colour->WithAlpha(0xFF));
		}
	}
	else
	{
		int i = -1;
		if (sim->photons[position.Y][position.X])
		{
			i = ID(sim->photons[position.Y][position.X]);
		}
		else if (sim->pmap[position.Y][position.X])
		{
			i = ID(sim->pmap[position.Y][position.X]);
		}
		if (i != -1)
		{
			auto *part = &sim->parts[i];
			if (game.GetSampleProperty())
			{
				auto *propTool = static_cast<PropertyTool *>(game.GetToolFromIdentifier("DEFAULT_UI_PROPERTY"));
				propTool->QueueTakePropertyFrom(i);
				game.SelectTool(0, propTool);
			}
			else if (part->type == PT_LIFE)
			{
				bool found = false;
				for (auto &tool : game.GetTools())
				{
					if (!tool)
					{
						continue;
					}
					if (tool->tool->Identifier.Contains("_PT_LIFE_") && ID(tool->tool->ToolID) == part->ctype)
					{
						game.SelectTool(0, tool->tool.get());
						found = true;
						break;
					}
				}
				if (!found)
				{
					static_cast<GOLTool *>(game.GetToolFromIdentifier("DEFAULT_UI_ADDLIFE"))->OpenWindow(0, part->ctype, RGB::Unpack(part->dcolour & 0xFFFFFF), RGB::Unpack(part->tmp & 0xFFFFFF));
				}
			}
			else
			{
				auto &sd = SimulationData::Ref();
				auto &elements = sd.elements;
				game.SelectTool(0, game.GetToolFromIdentifier(elements[part->type].Identifier));
			}
		}
	}
}
