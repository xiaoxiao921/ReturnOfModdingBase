#pragma once

#include <algorithm>
#include <assert.h>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <semaphore>
#include <thread>
#include <tuple>
#include <vector>

namespace big
{
	using Task = std::function<void()>;

	class thread_pool;
	inline thread_pool* g_thread_pool;

	// https://github.com/mhdshameel/WorkerPool
	class thread_pool
	{
	private:
		std::vector<std::thread> _threads;
		bool is_ready; //this is set thread safe using call_once
		bool cancel_flag;
		constexpr static size_t max_threads = 4;

		std::queue<std::tuple<Task, std::promise<void>>> task_queue;
		std::mutex tq_mx;

		std::unique_ptr<std::counting_semaphore<max_threads * max_threads>> pull_task_signal;
		/*
		* Not able to use condtion variable instead of semaphore for signalling the threads to pull tasks because of the lost wakeups
		* because sometimes we can have in this order: task addition to queue and then waiting on cv
		* */
		std::once_flag _isready_onceflag;
		std::atomic<int> available_workers;

	public:

		inline thread_pool() :
		    is_ready(0),
		    cancel_flag(false),
		    available_workers(0)
		{
			g_thread_pool = this;

			pull_task_signal = std::make_unique<std::counting_semaphore<max_threads * max_threads>>(0);
			_threads.reserve(max_threads);
			for (unsigned int i = 0; i < max_threads; i++)
			{
				_threads.emplace_back(&thread_pool::routine, this);
			}
		}

		thread_pool(const thread_pool&)            = delete; // cant allow copy as std::thread doesn't allow copy
		thread_pool(thread_pool&&)                 = default;
		thread_pool& operator=(const thread_pool&) = delete;
		thread_pool& operator=(thread_pool&&)      = default;

		/*
		* As soon as the object's destructor is invoked, the cancel signal is issued to all the worker threads.
		* And it will block till the current task of execution is completed
		* It is upto to the consumer of the library to not assign long blocking tasks
		* */
		inline ~thread_pool()
		{
			cancel_flag = true;
			pull_task_signal->release(_threads.size());

			//This is necessary, otherwise the abort is called. you can see in the std::thread's dtor
			for (auto& t : _threads)
			{
				t.join();
			}

			g_thread_pool = nullptr;
		}

		inline [[nodiscard]] bool is_thread_available()
		{
			return is_ready && (available_workers > 0);
		};

		inline [[nodiscard]] bool are_all_threads_available()
		{
			return available_workers == max_threads;
		}

		/*
		* Adds tasks to the internal queue. If workers are available immediately the task will be executed
		* If ready workers are not available, the tasks will be executed when any one worker thread is ready.
		*
		* Params:
		* task_to_run - This is the task to be executed. Any callable of signature void() will be accepted
		* */
		inline std::future<void> push(Task&& task_to_run)
		{
			// don't allow enqueueing after stopping the pool
			if (cancel_flag)
			{
				throw std::runtime_error("enqueue on stopped thread_pool");
			}

			std::promise<void> task_promise;
			auto fut = task_promise.get_future();
			std::unique_lock lk(tq_mx);
			task_queue.emplace(std::make_tuple(task_to_run, std::move(task_promise)));
			lk.unlock();
			pull_task_signal->release();
			return fut;
		}

	private:
		inline void routine() noexcept
		{
			std::call_once(
			    _isready_onceflag,
			    [](bool& is_ready)
			    {
				    is_ready = true;
			    },
			    is_ready);

			while (!(cancel_flag && task_queue.empty()))
			{
				available_workers++;
				try
				{
					if (!cancel_flag)
					{
						pull_task_signal->acquire();
					}

					std::unique_lock lk(tq_mx);
					available_workers--;
					if (task_queue.empty())
					{
						continue;
					}
					auto [payload, payload_promise] = std::exchange(task_queue.front(), {});
					/*
				The use of std::exchange ensures the task_queue's front element is accessed atomically and
				any potential data races between threads accessing task_queue are avoided
				*/

					task_queue.pop();
					lk.unlock();

					//execute the work assigned
					payload();

					payload_promise.set_value();
				}
				catch (const std::exception& ex)
				{
				}
			}
		}
	};
} // namespace big
