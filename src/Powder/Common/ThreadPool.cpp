#include "ThreadPool.hpp"
#include <cstdint>

namespace Powder
{
	namespace
	{
		// TODO-REDO_UI-FUTURE: use platform thread pool where available
		constexpr int32_t threadPoolSize = 4;
	}

	ThreadPool::ThreadPool()
	{
		threads.resize(threadPoolSize);
		for (auto &thread : threads)
		{
			thread = std::thread([this]() {
				Thread();
			});
		}
	}

	ThreadPool::~ThreadPool()
	{
		{
			std::unique_lock lk(mx);
			done = true;
		}
		cv.notify_all();
		for (auto &thread : threads)
		{
			thread.join();
		}
	}

	void ThreadPool::PushWorkItem(WorkItem workItem)
	{
		{
			std::unique_lock lk(mx);
			workItems.push_back(std::move(workItem));
		}
		cv.notify_all();
	}

	void ThreadPool::Thread()
	{
		while (true)
		{
			WorkItem workItem;
			while (true)
			{
				std::unique_lock lk(mx);
				cv.wait(lk, [this]() {
					return !workItems.empty() || done;
				});
				if (!workItems.empty())
				{
					workItem = std::move(workItems.front());
					workItems.pop_front();
					break;
				}
				if (done)
				{
					return;
				}
			}
			workItem();
		}
	}
}
