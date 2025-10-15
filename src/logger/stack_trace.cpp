#include "stack_trace.hpp"

#include "memory/module.hpp"

#include <DbgHelp.h>
#include <logger/logger.hpp>
#include <map>
#include <mutex>
#include <winternl.h>

namespace big
{
	stack_trace::stack_trace()
	{
		SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_DEBUG);

		// Get the directory of the current executable
		char exe_path[MAX_PATH];
		GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
		std::filesystem::path path(exe_path);
		std::string dir = path.parent_path().string();

		// Initialize with explicit search path
		if (!SymInitialize(GetCurrentProcess(), dir.c_str(), true))
		{
			DWORD error = GetLastError();
			LOG(WARNING) << "SymInitialize failed with error: " << error;
		}

		// Force load symbols for the main executable
		HMODULE hModule = GetModuleHandleA(nullptr);
		if (!SymLoadModuleEx(GetCurrentProcess(), nullptr, exe_path, nullptr, (DWORD64)hModule, 0, nullptr, 0))
		{
			DWORD error = GetLastError();
			if (error != ERROR_SUCCESS)
			{
				LOG(WARNING) << "SymLoadModuleEx failed with error: " << error;
			}
		}
	}

	stack_trace::~stack_trace()
	{
		SymCleanup(GetCurrentProcess());
	}

	const std::vector<uintptr_t>& stack_trace::frame_pointers()
	{
		return m_frame_pointers;
	}

	bool stack_trace::new_stack_trace(EXCEPTION_POINTERS* exception_info)
	{
		static std::mutex m;
		std::scoped_lock lock(m);

		m_exception_info = exception_info;

		m_dump << exception_code_to_string(exception_info->ExceptionRecord->ExceptionCode) << '\n';

		dump_module_info();
		dump_registers();
		dump_stacktrace();
		const auto should_be_silenced = dump_cpp_exception();
		if (should_be_silenced)
		{
			m_dump = {};
			return true;
		}

		m_dump << "\n--------End of exception--------\n";

		return false;
	}

	std::string stack_trace::str() const
	{
		return m_dump.str();
	}

	// I'd prefer to make some sort of global instance that cache all modules once instead of doing this every time
	void stack_trace::dump_module_info()
	{
		// modules cached already
		if (m_modules.size())
		{
			return;
		}

		m_dump << "Dumping modules:\n";

		const auto peb = reinterpret_cast<PPEB>(NtCurrentTeb()->ProcessEnvironmentBlock);
		if (!peb)
		{
			return;
		}

		const auto ldr_data = peb->Ldr;
		if (!ldr_data)
		{
			return;
		}

		const auto module_list = &ldr_data->InMemoryOrderModuleList;
		auto module_entry      = module_list->Flink;
		for (; module_list != module_entry; module_entry = module_entry->Flink)
		{
			const auto table_entry = CONTAINING_RECORD(module_entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
			if (!table_entry)
			{
				continue;
			}

			if (table_entry->FullDllName.Buffer)
			{
				auto mod_info = module_info(table_entry->FullDllName.Buffer, table_entry->DllBase);

				m_dump << mod_info.m_path.filename().string() << " Base Address: " << HEX_TO_UPPER(mod_info.m_base)
				       << " Size: " << mod_info.m_size << '\n';

				m_modules.emplace_back(std::move(mod_info));
			}
		}
	}

	void stack_trace::dump_registers()
	{
		const auto context = m_exception_info->ContextRecord;

#if defined(_WIN64)
		m_dump << "Dumping registers:\n"
		       << "RAX: " << HEX_TO_UPPER(context->Rax) << '\n'
		       << "RCX: " << HEX_TO_UPPER(context->Rcx) << '\n'
		       << "RDX: " << HEX_TO_UPPER(context->Rdx) << '\n'
		       << "RBX: " << HEX_TO_UPPER(context->Rbx) << '\n'
		       << "RSI: " << HEX_TO_UPPER(context->Rsi) << '\n'
		       << "RDI: " << HEX_TO_UPPER(context->Rdi) << '\n'
		       << "RSP: " << HEX_TO_UPPER(context->Rsp) << '\n'
		       << "RBP: " << HEX_TO_UPPER(context->Rbp) << '\n'
		       << "R8:  " << HEX_TO_UPPER(context->R8) << '\n'
		       << "R9:  " << HEX_TO_UPPER(context->R9) << '\n'
		       << "R10: " << HEX_TO_UPPER(context->R10) << '\n'
		       << "R11: " << HEX_TO_UPPER(context->R11) << '\n'
		       << "R12: " << HEX_TO_UPPER(context->R12) << '\n'
		       << "R13: " << HEX_TO_UPPER(context->R13) << '\n'
		       << "R14: " << HEX_TO_UPPER(context->R14) << '\n'
		       << "R15: " << HEX_TO_UPPER(context->R15) << '\n'
		       << "RIP: " << HEX_TO_UPPER(context->Rip) << '\n';
#elif defined(_WIN32)
		m_dump << "Dumping registers:\n"
		       << "EAX: " << HEX_TO_UPPER(context->Eax) << '\n'
		       << "ECX: " << HEX_TO_UPPER(context->Ecx) << '\n'
		       << "EDX: " << HEX_TO_UPPER(context->Edx) << '\n'
		       << "EBX: " << HEX_TO_UPPER(context->Ebx) << '\n'
		       << "ESI: " << HEX_TO_UPPER(context->Esi) << '\n'
		       << "EDI: " << HEX_TO_UPPER(context->Edi) << '\n'
		       << "ESP: " << HEX_TO_UPPER(context->Esp) << '\n'
		       << "EBP: " << HEX_TO_UPPER(context->Ebp) << '\n'
		       << "EIP: " << HEX_TO_UPPER(context->Eip) << '\n';
#endif
	}

	void stack_trace::dump_stacktrace()
	{
		m_dump << "Dumping stacktrace:";
		grab_stacktrace();

		char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
		auto symbol          = reinterpret_cast<SYMBOL_INFO*>(buffer);
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen   = MAX_SYM_NAME;

		DWORD64 displacement64;
		DWORD displacement;

		IMAGEHLP_LINE64 line;
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

		for (size_t i = 0; i < m_frame_pointers.size(); ++i)
		{
			const auto addr = m_frame_pointers[i];
			if (!addr)
			{
				continue;
			}

			m_dump << "\n[" << i << "]\t";

			if (SymFromAddr(GetCurrentProcess(), addr, &displacement64, symbol))
			{
				if (SymGetLineFromAddr64(GetCurrentProcess(), addr, &displacement, &line))
				{
					// Got file and line number information
					m_dump << line.FileName << ":" << line.LineNumber << " " << std::string_view(symbol->Name, symbol->NameLen);
				}
				else
				{
					const auto module_info = get_module_by_address(addr);
					if (module_info)
					{
						const auto module_name = module_info->m_path.filename().wstring();
						if (module_name == L"d3d12.dll")
						{
							m_dump << module_info->m_path.filename().string() << "+" << HEX_TO_UPPER(addr - module_info->m_base);
						}
						else
						{
							m_dump << module_info->m_path.filename().string() << " "
							       << std::string_view(symbol->Name, symbol->NameLen) << " ("
							       << module_info->m_path.filename().string() << "+" << HEX_TO_UPPER(addr - module_info->m_base) << ")";
						}
					}
					else
					{
						m_dump << std::string_view(symbol->Name, symbol->NameLen) << " [No module found for address " << HEX_TO_UPPER(addr) << "]";
					}
				}
			}
			else
			{
				// Failed to get symbol information
				DWORD sym_error = GetLastError();
				m_dump << "[SymFromAddr failed with error " << sym_error;

				// Translate common error codes
				switch (sym_error)
				{
				case ERROR_MOD_NOT_FOUND:   m_dump << " (ERROR_MOD_NOT_FOUND - Module not found)"; break;
				case ERROR_INVALID_ADDRESS: m_dump << " (ERROR_INVALID_ADDRESS - Invalid address)"; break;
				default:                    m_dump << " (Unknown error code)"; break;
				}
				m_dump << "] ";

				const auto module_info = get_module_by_address(addr);
				if (module_info)
				{
					m_dump << module_info->m_path.filename().string() << "+" << HEX_TO_UPPER(addr - module_info->m_base) << " " << HEX_TO_UPPER(addr);

					// Check if symbols are loaded for this module
					IMAGEHLP_MODULE64 mod_info{};
					mod_info.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
					if (SymGetModuleInfo64(GetCurrentProcess(), module_info->m_base, &mod_info))
					{
						m_dump << " [SymType: ";
						switch (mod_info.SymType)
						{
						case SymNone:     m_dump << "None - No symbols loaded"; break;
						case SymExport:   m_dump << "Export - Only export symbols"; break;
						case SymPdb:      m_dump << "PDB - Full PDB symbols"; break;
						case SymDeferred: m_dump << "Deferred - Symbols not loaded yet"; break;
						case SymCoff:     m_dump << "COFF"; break;
						case SymCv:       m_dump << "CodeView"; break;
						case SymDia:      m_dump << "DIA"; break;
						default:          m_dump << "Unknown (" << mod_info.SymType << ")"; break;
						}
						m_dump << ", LoadedPdbName: " << (mod_info.LoadedPdbName[0] ? mod_info.LoadedPdbName : "None") << "]";
					}
					else
					{
						m_dump << " [SymGetModuleInfo64 failed: " << GetLastError() << "]";
					}
				}
				else
				{
					m_dump << "No module found for address " << HEX_TO_UPPER(addr);
				}
			}
		}
	}

	// Raised by SetThreadName to notify the debugger thread names
	constexpr DWORD SetThreadName_exception_code = 0x40'6D'13'88;

	// Raised by MSVC C++ Exceptions
	constexpr DWORD msvc_exception_code = 0xE0'6D'73'63;

	[[nodiscard]] static const char* try_get_msvc_exception_what(_EXCEPTION_POINTERS* exp) noexcept
	{
		__try
		{
			return reinterpret_cast<std::exception*>(exp->ExceptionRecord->ExceptionInformation[1])->what();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
	}

	bool stack_trace::dump_cpp_exception()
	{
		if (m_exception_info->ExceptionRecord->ExceptionCode == msvc_exception_code)
		{
			const auto msvc_exception_what = try_get_msvc_exception_what(m_exception_info);
			if (msvc_exception_what)
			{
				m_dump << "\n\nC++ MSVC Exception: " << msvc_exception_what << '\n';
			}
			else
			{
				m_dump << "\n\nC++ MSVC Exception\n";
			}
		}
		else if (m_exception_info->ExceptionRecord->ExceptionCode == SetThreadName_exception_code)
		{
			LOG(INFO) << "SetThreadName Exception.";

			return true;
		}

		return false;
	}

	void stack_trace::grab_stacktrace()
	{
		CONTEXT context = *m_exception_info->ContextRecord;

		STACKFRAME64 frame{};
		frame.AddrPC.Mode    = AddrModeFlat;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrStack.Mode = AddrModeFlat;

#if defined(_WIN64)
		DWORD64 machineType    = IMAGE_FILE_MACHINE_AMD64;
		frame.AddrPC.Offset    = context.Rip;
		frame.AddrFrame.Offset = context.Rbp;
		frame.AddrStack.Offset = context.Rsp;
#elif defined(_WIN32)
		DWORD64 machineType    = IMAGE_FILE_MACHINE_I386;
		frame.AddrPC.Offset    = context.Eip;
		frame.AddrFrame.Offset = context.Ebp;
		frame.AddrStack.Offset = context.Esp;
#endif

		while (StackWalk64(machineType, GetCurrentProcess(), GetCurrentThread(), &frame, &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
		{
			m_frame_pointers.push_back(frame.AddrPC.Offset);
		}
	}

	const stack_trace::module_info* stack_trace::get_module_by_address(uintptr_t addr) const
	{
		for (auto& mod_info : m_modules)
		{
			if (mod_info.m_base < addr && addr < mod_info.m_base + mod_info.m_size)
			{
				return &mod_info;
			}
		}
		return nullptr;
	}

#define MAP_PAIR_STRINGIFY(x) {x, #x}
	static const std::map<DWORD, std::string> exceptions = {MAP_PAIR_STRINGIFY(EXCEPTION_ACCESS_VIOLATION), MAP_PAIR_STRINGIFY(EXCEPTION_ARRAY_BOUNDS_EXCEEDED), MAP_PAIR_STRINGIFY(EXCEPTION_DATATYPE_MISALIGNMENT), MAP_PAIR_STRINGIFY(EXCEPTION_FLT_DENORMAL_OPERAND), MAP_PAIR_STRINGIFY(EXCEPTION_FLT_DIVIDE_BY_ZERO), MAP_PAIR_STRINGIFY(EXCEPTION_FLT_INEXACT_RESULT), MAP_PAIR_STRINGIFY(EXCEPTION_FLT_INEXACT_RESULT), MAP_PAIR_STRINGIFY(EXCEPTION_FLT_INVALID_OPERATION), MAP_PAIR_STRINGIFY(EXCEPTION_FLT_OVERFLOW), MAP_PAIR_STRINGIFY(EXCEPTION_FLT_STACK_CHECK), MAP_PAIR_STRINGIFY(EXCEPTION_FLT_UNDERFLOW), MAP_PAIR_STRINGIFY(EXCEPTION_ILLEGAL_INSTRUCTION), MAP_PAIR_STRINGIFY(EXCEPTION_IN_PAGE_ERROR), MAP_PAIR_STRINGIFY(EXCEPTION_INT_DIVIDE_BY_ZERO), MAP_PAIR_STRINGIFY(EXCEPTION_INT_OVERFLOW), MAP_PAIR_STRINGIFY(EXCEPTION_INVALID_DISPOSITION), MAP_PAIR_STRINGIFY(EXCEPTION_NONCONTINUABLE_EXCEPTION), MAP_PAIR_STRINGIFY(EXCEPTION_PRIV_INSTRUCTION), MAP_PAIR_STRINGIFY(EXCEPTION_STACK_OVERFLOW), MAP_PAIR_STRINGIFY(EXCEPTION_BREAKPOINT), MAP_PAIR_STRINGIFY(EXCEPTION_SINGLE_STEP)};

	std::string stack_trace::exception_code_to_string(const DWORD code)
	{
		if (const auto& it = exceptions.find(code); it != exceptions.end())
		{
			return it->second;
		}

		if (code == msvc_exception_code)
		{
			return "C++ MSVC Exception";
		}

		if (code == SetThreadName_exception_code)
		{
			return "SetThreadName Exception";
		}

		std::stringstream exception_code_str;
		exception_code_str << HEX_TO_UPPER(code);
		return "UNKNOWN_EXCEPTION: CODE: " + exception_code_str.str();
	}
} // namespace big
