#pragma once
#include "simulation/ElementDefs.h"

int Element_FILT_getWavelengths(const Particle* cpart);
int Element_FILT_interactWavelengths(RNG &rng, Particle* cpart, int origWl);
