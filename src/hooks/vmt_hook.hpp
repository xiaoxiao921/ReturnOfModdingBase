#pragma once

#include <cstddef>
#include <memory>

namespace big
{
	class vmt_hook
	{
	public:
		explicit vmt_hook();
		explicit vmt_hook(void* obj, std::size_t num_funcs);
		~vmt_hook();

		void init(void* obj, std::size_t num_funcs);

		vmt_hook(vmt_hook&& that)            = delete;
		vmt_hook& operator=(vmt_hook&& that) = delete;
		vmt_hook(const vmt_hook&)            = delete;
		vmt_hook& operator=(const vmt_hook&) = delete;

		void hook(std::size_t index, void* func);
		void unhook(std::size_t index);

		template<typename T>
		T get_original(std::size_t index);

		void enable();
		void disable();

	private:
		void*** m_object;
		std::size_t m_num_funcs;

		void** m_original_table;
		std::unique_ptr<void*[]> m_new_table;
	};

	template<typename T>
	inline T vmt_hook::get_original(std::size_t index)
	{
		return static_cast<T>(m_original_table[index]);
	}
} // namespace big
