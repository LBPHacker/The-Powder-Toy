#pragma once
#include "ElementDefs.h"

extern float IPL;
extern float IPH;
extern float ITL;
extern float ITH;

// no transition (PT_NONE means kill part)
constexpr int NT = -1;

// special transition - lava ctypes etc need extra code, which is only found and run if ST is given
constexpr int ST = PT_NUM;
