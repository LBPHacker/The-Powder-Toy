#pragma once
#include "StructProperty.h"
#include <vector>

struct Particle
{
	int type;
	int life, ctype;
	float x, y, vx, vy;
	float temp;
	int tmp3;
	int tmp4;
	int flags;
	int tmp;
	int tmp2;
	unsigned int dcolour;
	/** Returns a list of properties, their type and offset within the structure that can be changed
	 by higher-level processes referring to them by name such as Lua or the property tool **/
	static const std::vector<StructProperty> &GetProperties();
	static const std::vector<StructPropertyAlias> &GetPropertyAliases();
	static const std::vector<unsigned int> &PossiblyCarriesType();
};

// important: these are indices into the vector returned by Particle::GetProperties, not indices into Particle
constexpr unsigned int FIELD_TYPE = 0;
constexpr unsigned int FIELD_LIFE = 1;
constexpr unsigned int FIELD_CTYPE = 2;
constexpr unsigned int FIELD_TMP = 9;
constexpr unsigned int FIELD_TMP2 = 10;
constexpr unsigned int FIELD_TMP3 = 11;
constexpr unsigned int FIELD_TMP4 = 12;
