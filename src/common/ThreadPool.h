#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class ThreadPool
{
public:
	// using WorkItem = std::move_only_function<void ()>; // C++23 >_>
	struct WorkItem
	{
		struct WrapperBase
		{
			virtual void operator ()() = 0;

			virtual ~WrapperBase() = default;
		};
		std::unique_ptr<WrapperBase> wrapper;

		WorkItem() = default;

		template<class Func, class = std::enable_if_t<!std::is_same_v<Func, WorkItem>, int>>
		WorkItem(Func func)
		{
			struct Wrapper : public WrapperBase
			{
				Func func;

				Wrapper(Func newFunc) : func(std::move(newFunc))
				{
				}

				void operator ()() final override
				{
					func();
				}
			};
			wrapper = std::make_unique<Wrapper>(std::move(func));
		}

		void operator ()()
		{
			(*wrapper)();
		}
	};

private:
	std::deque<WorkItem> workItems;
	int inProgress = 0;
	struct ThreadContext
	{
		std::thread thr;
		bool done = false;
	};
	std::vector<ThreadContext> threads;
	std::condition_variable cv;
	std::mutex mx;

	void Thread(int index);

public:
	~ThreadPool();
	void PushWorkItem(WorkItem workItem);
	void Flush();
	void SetThreadCount(int newThreadCount);

	int GetThreadCount() const
	{
		return int(threads.size());
	}
};
