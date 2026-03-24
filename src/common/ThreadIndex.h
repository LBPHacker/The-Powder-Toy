#pragma once

inline int &ThreadIndex()
{
	thread_local int threadIndex = -1;
	return threadIndex;
}
