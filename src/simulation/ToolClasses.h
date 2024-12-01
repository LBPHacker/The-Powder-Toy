#pragma once
#include <memory>
#include <vector>

#include "SimTool.h"

#define TOOL_NUMBERS_ENUMERATE
#include "ToolNumbers.h"
#undef TOOL_NUMBERS_ENUMERATE

const std::vector<SimTool> &GetTools();
