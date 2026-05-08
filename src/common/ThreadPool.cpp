#include "ThreadPool.h"
#include "ThreadIndex.h"

ThreadPool::~ThreadPool()
{
	SetThreadCount(0);
}

void ThreadPool::PushWorkItem(WorkItem workItem)
{
	{
		std::unique_lock lk(mx);
		workItems.push_back(std::move(workItem));
	}
	cv.notify_all();
}

void ThreadPool::Thread(int index)
{
	ThreadIndex() = index;
	while (true)
	{
		CopyableWorkItem doFirst;
		WorkItem workItem;
		{
			std::unique_lock lk(mx);
			cv.wait(lk, [this, index]() {
				return threads[index].doFirst || !workItems.empty() || threads[index].done;
			});
			if (threads[index].doFirst)
			{
				std::swap(doFirst, threads[index].doFirst);
			}
			else if (!workItems.empty())
			{
				workItem = std::move(workItems.front());
				workItems.pop_front();
			}
			else if (threads[index].done)
			{
				return;
			}
			inProgress += 1;
		}
		if (doFirst)
		{
			doFirst();
		}
		else
		{
			workItem();
		}
		{
			std::unique_lock lk(mx);
			inProgress -= 1;
		}
		cv.notify_all();
	}
}

void ThreadPool::DoFirstOnAllThreads(CopyableWorkItem doFirst)
{
	{
		std::unique_lock lk(mx);
		for (auto &context : threads)
		{
			context.doFirst = doFirst;
		}
	}
	cv.notify_all();
}

void ThreadPool::SetThreadCount(int newThreadCount)
{
	auto oldThreadCount = int(threads.size());
	if (newThreadCount == oldThreadCount)
	{
		return;
	}
	{
		std::unique_lock lk(mx);
		for (int i = newThreadCount; i < oldThreadCount; ++i)
		{
			threads[i].done = true;
		}
	}
	cv.notify_all();
	for (int i = newThreadCount; i < oldThreadCount; ++i)
	{
		threads[i].thr.join();
	}
	{
		std::unique_lock lk(mx);
		threads.resize(newThreadCount);
	}
	for (int i = oldThreadCount; i < newThreadCount; ++i)
	{
		threads[i].thr = std::thread([this, i]() {
			Thread(i);
		});
	}
}

void ThreadPool::Flush()
{
	std::unique_lock lk(mx);
	cv.wait(lk, [this]() {
		for (auto &context : threads)
		{
			if (context.doFirst)
			{
				return false;
			}
		}
		return workItems.empty() && inProgress == 0;
	});
}
