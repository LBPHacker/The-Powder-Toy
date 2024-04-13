#pragma once
#include <array>

constexpr auto MAX_FIGHTERS = 100;
struct playerst
{
	char comm  = 0;           //command cell
	char pcomm = 0;          //previous command
	int elem   = 0;            //element power
	std::array<float, 16> legs{};      //legs' positions
	std::array<float, 8> accs{};       //accelerations
	char spwn           = 0;           //if stick man was spawned
	unsigned int frames = 0; //frames since last particle spawn - used when spawning LIGH
	bool rocketBoots    = 0;
	bool fan            = 0;
	int spawnID         = 0;         //id of the SPWN particle that spawns it
};
