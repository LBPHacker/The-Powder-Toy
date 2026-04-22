#pragma once
#include "DebugInfo.h"

class GameModel;
class GameView;
class GameController;
class AirVelocity : public DebugInfo
{
	GameModel *model;
	GameView *view;
	GameController *controller;

public:
	AirVelocity(unsigned int id, GameModel *model, GameView *newView, GameController *newController);

	void Draw() override;
};
