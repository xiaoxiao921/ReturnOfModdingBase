#include "thread_pool.hpp"

namespace big
{
	thread_pool::thread_pool(size_t num_threads) :
	    m_stop(false)
	{
		for (size_t i = 0; i < num_threads; ++i)
		{
			m_workers.emplace_back(
			    [this]
			    {
				    while (true)
				    {
					    std::function<void()> job;
					    {
						    std::unique_lock<std::mutex> lock(m_queue_mutex);
						    m_condition.wait(lock,
						                     [this]
						                     {
							                     return m_stop || !m_jobs.empty();
						                     });

						    if (m_stop && m_jobs.empty())
						    {
							    return;
						    }

						    job = std::move(m_jobs.front());
						    m_jobs.pop();
					    }
					    job();
				    }
			    });
		}
	}

	thread_pool::~thread_pool()
	{
		{
			std::unique_lock<std::mutex> lock(m_queue_mutex);
			m_stop = true;
		}
		m_condition.notify_all();
		for (std::thread& worker : m_workers)
		{
			worker.join();
		}
	}

	void thread_pool::enqueue(std::function<void()> job)
	{
		{
			std::unique_lock<std::mutex> lock(m_queue_mutex);
			m_jobs.push(std::move(job));
		}
		m_condition.notify_one();
	}
} // namespace big
