#pragma once

#include "DebugInfo.h"

class GameModel;
class DebugParts : public DebugInfo
{
	GameModel * model;
public:
	DebugParts(unsigned int id, GameModel * model);
	void Draw() override;
	virtual ~DebugParts();
};
