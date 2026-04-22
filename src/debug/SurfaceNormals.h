#pragma once
#include "DebugInfo.h"

class GameModel;
class GameView;
class GameController;
class SurfaceNormals : public DebugInfo
{
	GameModel *model;
	GameView *view;
	GameController *controller;

public:
	SurfaceNormals(unsigned int id, GameModel *model, GameView *newView, GameController *newController);

	void Draw() override;
};
