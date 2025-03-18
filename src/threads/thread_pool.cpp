#include "thread_pool.hpp"

#include "logger/logger.hpp"

#undef ERROR

namespace big
{
	thread_pool::thread_pool(const std::size_t preallocated_thread_count) :
	    m_accept_jobs(true),
	    m_allocated_thread_count(preallocated_thread_count),
	    m_busy_threads(0)
	{
		m_thread_pool.reserve(m_allocated_thread_count);

		if (m_thread_pool.size() < m_allocated_thread_count)
		{
			for (auto i = m_thread_pool.size(); i < m_allocated_thread_count; i++)
			{
				m_thread_pool.emplace_back(std::thread(&thread_pool::run, this));
			}
		}

		g_thread_pool = this;
	}

	thread_pool::~thread_pool()
	{
		g_thread_pool = nullptr;
	}

	void thread_pool::destroy()
	{
		{
			std::unique_lock lock(m_lock);
			m_accept_jobs = false;
		}
		m_data_condition.notify_all();

		for (auto& thread : m_thread_pool)
		{
			thread.join();
		}

		m_thread_pool.clear();
	}

	void thread_pool::push(std::function<void()> func, std::source_location location)
	{
		if (func)
		{
			{
				std::unique_lock lock(m_lock);
				m_job_stack.push({func, location});
			}
			m_data_condition.notify_all();
		}
	}

	void thread_pool::run()
	{
		for (;;)
		{
			std::unique_lock lock(m_lock);
			m_data_condition.wait(lock,
			                      [this]()
			                      {
				                      return !m_job_stack.empty() || !m_accept_jobs;
			                      });

			if (!m_accept_jobs) [[unlikely]]
			{
				break;
			}
			if (m_job_stack.empty()) [[likely]]
			{
				continue;
			}

			thread_pool_job job = m_job_stack.top();
			m_job_stack.pop();
			lock.unlock();

			++m_busy_threads;

			try
			{
				std::invoke(job.m_func);
			}
			catch (const std::exception& e)
			{
				LOG(WARNING) << "Exception thrown while executing job in thread:" << std::endl << e.what();
			}

			--m_busy_threads;
		}
	}
} // namespace big
