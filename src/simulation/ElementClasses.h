#pragma once
#include <array>

#include "Element.h"
#include "SimulationData.h"

#define ELEMENT_NUMBERS_ENUMERATE
#include "ElementNumbers.h"
#undef ELEMENT_NUMBERS_ENUMERATE

const std::array<Element, PT_NUM> &GetElements();
