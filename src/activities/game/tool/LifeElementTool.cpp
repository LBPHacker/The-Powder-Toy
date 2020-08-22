#include "LifeElementTool.h"

#include "simulation/ElementClasses.h"

namespace activities
{
namespace game
{
namespace tool
{
	LifeElementTool::LifeElementTool(int kind, String name, String description, gui::Color color) :
		ElementTool(PMAP(kind, PT_LIFE)),
		name(name),
		description(description),
		color(color)
	{
	}
}
}
}
