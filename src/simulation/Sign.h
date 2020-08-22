#ifndef SIGN_H_
#define SIGN_H_
#include "Config.h"

#include "common/String.h"

#include <utility>

class Simulation;

struct sign
{
	enum Justification
	{
		Left = 0,
		Middle = 1,
		Right = 2,
		None = 3
	};

	enum Type
	{
		Normal,
		Save,
		Thread,
		Button,
		Search
	};

	int x, y;
	Justification ju;
	String text;

	sign(String text_, int x_, int y_, Justification justification_);

	struct Info
	{
		bool v95;
		int x, y, w, h;
		int r, g, b;
		String text;
	};

	Info GetInfo(const Simulation *sim) const;
	std::pair<int, Type> split() const;
};

#endif
