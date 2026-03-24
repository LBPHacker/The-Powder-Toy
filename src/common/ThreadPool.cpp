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
		WorkItem workItem;
		while (true)
		{
			std::unique_lock lk(mx);
			cv.wait(lk, [this, index]() {
				return !workItems.empty() || threads[index].done;
			});
			if (!workItems.empty())
			{
				workItem = std::move(workItems.front());
				workItems.pop_front();
				inProgress += 1;
				break;
			}
			if (threads[index].done)
			{
				return;
			}
		}
		workItem();
		{
			std::unique_lock lk(mx);
			inProgress -= 1;
		}
		cv.notify_all();
	}
}

void ThreadPool::SetThreadCount(int newThreadCount)
{
	auto oldThreadCount = int(threads.size());
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
		return workItems.empty() && inProgress == 0;
	});
}
