#pragma once
#include "simulation/ElementDefs.h"

int Element_FILT_getWavelengths(const Particle* cpart);
int Element_FILT_interactWavelengths(Simulation *sim, Particle* cpart, int origWl);
std::tuple<int, int, int> Element_FILT_wavelengthsToColor(int wl);
