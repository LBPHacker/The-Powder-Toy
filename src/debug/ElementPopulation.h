#pragma once

#include "DebugInfo.h"

class GameModel;
class ElementPopulationDebug : public DebugInfo
{
	GameModel * model;
	float maxAverage;
public:
	ElementPopulationDebug(unsigned int id, GameModel * model);
	void Draw() override;
	virtual ~ElementPopulationDebug();
};
