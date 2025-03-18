#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <source_location>
#include <stack>
#include <vector>

namespace big
{
	class thread_pool
	{
	private:
		std::vector<std::thread> m_workers;
		std::queue<std::function<void()>> m_jobs;
		std::mutex m_queue_mutex;
		std::condition_variable m_condition;
		std::atomic<bool> m_stop;

	public:
		thread_pool(size_t num_threads = 4);

		~thread_pool();

		void enqueue(std::function<void()> job);
	};

	inline thread_pool* g_thread_pool{};
} // namespace big
