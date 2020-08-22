#pragma once

#include "Component.h"

namespace gui
{
	class Separator : public Component
	{
	public:
		void Draw() const final override;
	};
}
