#include "Graphics.h"
#include "RasterDrawMethodsImpl.h"
#include "SimulationConfig.h"
#include <cstdlib>
#include <cstring>

void Graphics::Finalise()
{
}

template struct RasterDrawMethods<Graphics>;
