#pragma once
#include "range.hpp"

#include <chrono>
#include <string_view>
#include <Windows.h>

namespace memory
{
	class module : public range
	{
	public:
		explicit module(const std::string_view name);

		/**
		 * @brief Get the export address of the current module given a symbol name
		 * 
		 * @param symbol_name 
		 * @return memory::handle 
		 */
		memory::handle get_export(std::string_view symbol_name);

		bool loaded() const;
		size_t size() const;
		DWORD timestamp() const;

		/**
		 * @brief Waits till the given module is loaded.
		 * 
		 * @param time Time to wait before giving up.
		 * @return true 
		 * @return false 
		 */
		bool wait_for_module(std::optional<std::chrono::high_resolution_clock::duration> time = std::nullopt);

	protected:
		bool try_get_module();

	private:
		const std::string_view m_name;
		bool m_loaded;

	public:
		template<class F>
		bool for_each_imports(HMODULE module, const char* dll_name, const F& f)
		{
			if (module == 0)
			{
				return false;
			}

			uintptr_t image_base_address = (uintptr_t)module;

			PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)image_base_address;
			if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
			{
				return false;
			}

			PIMAGE_NT_HEADERS image_nt_headers = (PIMAGE_NT_HEADERS)(image_base_address + dos_header->e_lfanew);

			uintptr_t rva_imports = image_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
			if (rva_imports == 0)
			{
				return false;
			}

			IMAGE_IMPORT_DESCRIPTOR* import_descriptor = (IMAGE_IMPORT_DESCRIPTOR*)(image_base_address + rva_imports);
			while (import_descriptor->Name != 0)
			{
				if (!dll_name || stricmp((const char*)(image_base_address + import_descriptor->Name), dll_name) == 0)
				{
					IMAGE_IMPORT_BY_NAME** func_names = (IMAGE_IMPORT_BY_NAME**)(image_base_address + import_descriptor->Characteristics);

					void** import_table = (void**)(image_base_address + import_descriptor->FirstThunk);

					for (uintptr_t i = 0;; ++i)
					{
						if ((uintptr_t)func_names[i] == 0)
						{
							break;
						}

						const char* func_name = (const char*)(image_base_address + (uintptr_t)func_names[i]->Name);

						f(func_name, import_table[i]);
					}
				}

				++import_descriptor;
			}

			return true;
		}
	};
} // namespace memory
