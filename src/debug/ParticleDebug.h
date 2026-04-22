#pragma once
#include "DebugInfo.h"

class GameModel;
class ParticleDebug : public DebugInfo
{
	GameModel * model;
public:
	ParticleDebug(unsigned int id, GameModel * model);
	void Debug(int mode, int x, int y);
	bool KeyPress(int key, int scan, bool shift, bool ctrl, bool alt, ui::Point currentMouse) override;
};
