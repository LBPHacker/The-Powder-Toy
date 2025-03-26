#pragma once
#include <functional>
#include <deque>
#include <thread>
#include <vector>
#include <condition_variable>
#include <mutex>
// #include <functional> // for std::move_only_function

namespace Powder
{
	class ThreadPool
	{
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

		std::deque<WorkItem> workItems;
		std::vector<std::thread> threads;
		std::condition_variable cv;
		std::mutex mx;
		bool done = false;

		void Thread();

	public:
		ThreadPool();
		~ThreadPool();
		void PushWorkItem(WorkItem workItem);
	};
}
