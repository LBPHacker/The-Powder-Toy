/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <cstdlib>
#include <exception>
#include <vector>
#include <array>
#include "SimulationConfig.h"

class CoordStackOverflowException: public std::exception
{
public:
	CoordStackOverflowException() { }
	const char* what() const throw() override
	{
		return "Maximum number of entries in the coordinate stack was exceeded";
	}
	~CoordStackOverflowException() throw() {}
};

class CoordStack
{
private:
	std::vector<std::array<unsigned short, 2>> stack;
	int stack_size;
public:
	CoordStack() :
		stack(XRES*YRES),
		stack_size(0)
	{
	}
	void push(int x, int y)
	{
		if (stack_size>=XRES*YRES)
			throw CoordStackOverflowException();
		stack[stack_size][0] = x;
		stack[stack_size][1] = y;
		stack_size++;
	}
	void pop(int& x, int& y)
	{
		stack_size--;
		x = stack[stack_size][0];
		y = stack[stack_size][1];
	}
	int getSize() const
	{
		return stack_size;
	}
	void clear()
	{
		stack_size = 0;
	}
};
