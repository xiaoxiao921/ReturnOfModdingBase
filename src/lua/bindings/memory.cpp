#include "memory.hpp"

#include "lua/lua_manager.hpp"
#include "memory/module.hpp"
#include "memory/pattern.hpp"
#include "rom/rom.hpp"

#include <asmjit/asmjit.h>
#include <hooks/detour_hook.hpp>

// clang-format off
#include <AsyncLogger/Logger.hpp>
using namespace al;

// clang-format on

namespace lua::memory
{
	pointer::pointer(uintptr_t address) :
	    m_address(address)
	{
	}

	pointer::pointer() :
	    m_address(0)
	{
	}

	pointer pointer::add(uintptr_t offset)
	{
		return pointer(m_address + offset);
	}

	pointer pointer::sub(uintptr_t offset)
	{
		return pointer(m_address - offset);
	}

	pointer pointer::rip()
	{
		return add(*(std::int32_t*)m_address).add(4);
	}

	pointer pointer::rip_cmp()
	{
		return rip().add(1);
	}

	std::string pointer::get_string()
	{
		return std::string((char*)m_address);
	}

	void pointer::set_string(const std::string& string, int max_length)
	{
		strncpy((char*)m_address, string.data(), max_length);
	}

	bool pointer::is_null()
	{
		return m_address == 0;
	}

	bool pointer::is_valid()
	{
		return !is_null();
	}

	pointer pointer::deref()
	{
		return pointer(*(uintptr_t*)m_address);
	}

	uintptr_t pointer::get_address() const
	{
		return m_address;
	}

	// Lua API: Table
	// Name: memory
	// Table containing helper functions related to process memory.

	// Lua API: Function
	// Table: memory
	// Name: scan_pattern
	// Param: pattern: string: byte pattern (IDA format)
	// Returns: pointer: A pointer to the found address.
	// Scans the specified memory pattern within the target main module and returns a pointer to the found address.
	static pointer scan_pattern(const std::string& pattern)
	{
		return pointer(::memory::module(rom::g_target_module_name).scan(::memory::pattern(pattern)).value().as<uintptr_t>());
	}

	// Lua API: Function
	// Table: memory
	// Name: allocate
	// Param: size: integer: The number of bytes to allocate on the heap.
	// Returns: pointer: A pointer to the newly allocated memory.
	static pointer allocate(int size, sol::this_environment env)
	{
		void* mem = new uint8_t[size]();

		big::lua_module* module = big::lua_module::this_from(env);
		if (module)
		{
			module->m_data.m_allocated_memory.push_back(mem);
		}

		return pointer((uintptr_t)mem);
	}

	// Lua API: Function
	// Table: memory
	// Name: free
	// Param: ptr: pointer: The pointer that must be freed.
	static void free(pointer ptr, sol::this_environment env)
	{
		delete[] (void*)ptr.get_address();
		big::lua_module* module = big::lua_module::this_from(env);
		if (module)
		{
			std::erase_if(module->m_data.m_allocated_memory,
			              [ptr](void* addr)
			              {
				              return ptr.get_address() == (uintptr_t)addr;
			              });
		}
	}

#define TYPEID_MATCH_STR_IF(var, T)                                      \
	if (var == #T)                                                       \
	{                                                                    \
		return asmjit::TypeId(asmjit::TypeUtils::TypeIdOfT<T>::kTypeId); \
	}
#define TYPEID_MATCH_STR_ELSEIF(var, T)                                  \
	else if (var == #T)                                                  \
	{                                                                    \
		return asmjit::TypeId(asmjit::TypeUtils::TypeIdOfT<T>::kTypeId); \
	}

	// does a given type fit in a general purpose register (i.e. is it integer type)
	bool is_general_register(const asmjit::TypeId type_id)
	{
		switch (type_id)
		{
		case asmjit::TypeId::kInt8:
		case asmjit::TypeId::kUInt8:
		case asmjit::TypeId::kInt16:
		case asmjit::TypeId::kUInt16:
		case asmjit::TypeId::kInt32:
		case asmjit::TypeId::kUInt32:
		case asmjit::TypeId::kInt64:
		case asmjit::TypeId::kUInt64:
		case asmjit::TypeId::kIntPtr:
		case asmjit::TypeId::kUIntPtr: return true;
		default:                       return false;
		}
	}

	// float, double, simd128
	bool is_XMM_register(const asmjit::TypeId type_id)
	{
		switch (type_id)
		{
		case asmjit::TypeId::kFloat32:
		case asmjit::TypeId::kFloat64: return true;
		default:                       return false;
		}
	}

	asmjit::CallConvId get_call_convention(const std::string& conv)
	{
		if (conv == "cdecl")
		{
			return asmjit::CallConvId::kCDecl;
		}
		else if (conv == "stdcall")
		{
			return asmjit::CallConvId::kStdCall;
		}
		else if (conv == "fastcall")
		{
			return asmjit::CallConvId::kFastCall;
		}

		return asmjit::CallConvId::kHost;
	}

	asmjit::TypeId get_type_id(const std::string& type)
	{
		if (type.find('*') != std::string::npos)
		{
			return asmjit::TypeId::kUIntPtr;
		}

		TYPEID_MATCH_STR_IF(type, signed char)
		TYPEID_MATCH_STR_ELSEIF(type, unsigned char)
		TYPEID_MATCH_STR_ELSEIF(type, short)
		TYPEID_MATCH_STR_ELSEIF(type, unsigned short)
		TYPEID_MATCH_STR_ELSEIF(type, int)
		TYPEID_MATCH_STR_ELSEIF(type, unsigned int)
		TYPEID_MATCH_STR_ELSEIF(type, long)
		TYPEID_MATCH_STR_ELSEIF(type, unsigned long)
#ifdef POLYHOOK2_OS_WINDOWS
		TYPEID_MATCH_STR_ELSEIF(type, __int64)
		TYPEID_MATCH_STR_ELSEIF(type, unsigned __int64)
#endif
		TYPEID_MATCH_STR_ELSEIF(type, long long)
		TYPEID_MATCH_STR_ELSEIF(type, unsigned long long)
		TYPEID_MATCH_STR_ELSEIF(type, char)
		TYPEID_MATCH_STR_ELSEIF(type, char16_t)
		TYPEID_MATCH_STR_ELSEIF(type, char32_t)
		TYPEID_MATCH_STR_ELSEIF(type, wchar_t)
		TYPEID_MATCH_STR_ELSEIF(type, uint8_t)
		TYPEID_MATCH_STR_ELSEIF(type, int8_t)
		TYPEID_MATCH_STR_ELSEIF(type, uint16_t)
		TYPEID_MATCH_STR_ELSEIF(type, int16_t)
		TYPEID_MATCH_STR_ELSEIF(type, int32_t)
		TYPEID_MATCH_STR_ELSEIF(type, uint32_t)
		TYPEID_MATCH_STR_ELSEIF(type, uint64_t)
		TYPEID_MATCH_STR_ELSEIF(type, int64_t)
		TYPEID_MATCH_STR_ELSEIF(type, float)
		TYPEID_MATCH_STR_ELSEIF(type, double)
		TYPEID_MATCH_STR_ELSEIF(type, bool)
		TYPEID_MATCH_STR_ELSEIF(type, void)
		else if (type == "intptr_t")
		{
			return asmjit::TypeId::kIntPtr;
		}
		else if (type == "uintptr_t")
		{
			return asmjit::TypeId::kUIntPtr;
		}

		return asmjit::TypeId::kVoid;
	}

	static type_info_t get_type_info_from_string(const std::string& s)
	{
		if ((s.contains("const") && s.contains("char") && s.contains("*")) || s.contains("string"))
		{
			return type_info_t::string_;
		}
		else if (s.contains("bool"))
		{
			return type_info_t::boolean_;
		}
		else if (s.contains("ptr") || s.contains("pointer") || s.contains("*"))
		{
			// passing lua::memory::pointer
			return type_info_t::ptr_;
		}
		else if (s.contains("float"))
		{
			return type_info_t::float_;
		}
		else if (s.contains("double"))
		{
			return type_info_t::double_;
		}
		else
		{
			return type_info_t::integer_;
		}
	}

	class runtime_func_t
	{
		std::vector<uint8_t> m_jit_function_buffer;
		asmjit::x86::Mem m_args_stack;

		std::unique_ptr<big::detour_hook> m_detour;

	public:
		type_info_t m_return_type;
		std::vector<type_info_t> m_param_types;

		struct parameters_t
		{
			template<typename T>
			void set(const uint8_t idx, const T val) const
			{
				*(T*)get_arg_ptr(idx) = val;
			}

			template<typename T>
			T get(const uint8_t idx) const
			{
				return *(T*)get_arg_ptr(idx);
			}

			// asm depends on this specific type
			// we the runtime_func allocates stack space that is set to point here (check asmjit::compiler.newStack calls)
			volatile uintptr_t m_arguments;

			// must be char* for aliasing rules to work when reading back out
			char* get_arg_ptr(const uint8_t idx) const
			{
				return ((char*)&m_arguments) + sizeof(uintptr_t) * idx;
			}
		};

		class return_value_t
		{
			uintptr_t m_return_value;

		public:
			unsigned char* get() const
			{
				return (unsigned char*)&m_return_value;
			}
		};

		typedef bool (*user_pre_callback_t)(const parameters_t* params, const uint8_t parameters_count, return_value_t* return_value, const uintptr_t target_func_ptr);
		typedef void (*user_post_callback_t)(const parameters_t* params, const uint8_t parameters_count, return_value_t* return_value, const uintptr_t target_func_ptr);

		runtime_func_t()
		{
			m_detour      = std::make_unique<big::detour_hook>();
			m_return_type = type_info_t::none_;
		}

		~runtime_func_t()
		{
		}

		// Construct a callback given the raw signature at runtime. 'Callback' param is the C stub to transfer to,
		// where parameters can be modified through a structure which is written back to the parameter slots depending
		// on calling convention.
		uintptr_t make_jit_func(const asmjit::FuncSignature& sig, const asmjit::Arch arch, const user_pre_callback_t pre_callback, const user_post_callback_t post_callback, const uintptr_t target_func_ptr)
		{
			asmjit::CodeHolder code;
			auto env = asmjit::Environment::host();
			env.setArch(arch);
			code.init(env);

			// initialize function
			asmjit::x86::Compiler cc(&code);
			asmjit::FuncNode* func = cc.addFunc(sig);

			asmjit::StringLogger log;
			// clang-format off
			const auto format_flags =
				asmjit::FormatFlags::kMachineCode | asmjit::FormatFlags::kExplainImms | asmjit::FormatFlags::kRegCasts |
				asmjit::FormatFlags::kHexImms     | asmjit::FormatFlags::kHexOffsets  | asmjit::FormatFlags::kPositions;
			// clang-format on

			log.addFlags(format_flags);
			code.setLogger(&log);

			// too small to really need it
			func->frame().resetPreservedFP();

			// map argument slots to registers, following abi.
			std::vector<asmjit::x86::Reg> arg_registers;
			for (uint8_t arg_index = 0; arg_index < sig.argCount(); arg_index++)
			{
				const auto arg_type = sig.args()[arg_index];

				asmjit::x86::Reg arg;
				if (is_general_register(arg_type))
				{
					arg = cc.newUIntPtr();
				}
				else if (is_XMM_register(arg_type))
				{
					arg = cc.newXmm();
				}
				else
				{
					LOG(ERROR) << "Parameters wider than 64bits not supported";
					return 0;
				}

				func->setArg(arg_index, arg);
				arg_registers.push_back(arg);
			}

			// setup the stack structure to hold arguments for user callback
			uint32_t stack_size = (uint32_t)(sizeof(uintptr_t) * sig.argCount());
			m_args_stack        = cc.newStack(stack_size, 16);
			asmjit::x86::Mem args_stack_index(m_args_stack);

			// assigns some register as index reg
			asmjit::x86::Gp i = cc.newUIntPtr();

			// stack_index <- stack[i].
			args_stack_index.setIndex(i);

			// r/w are sizeof(uintptr_t) width now
			args_stack_index.setSize(sizeof(uintptr_t));

			// set i = 0
			cc.mov(i, 0);
			// mov from arguments registers into the stack structure
			for (uint8_t argIdx = 0; argIdx < sig.argCount(); argIdx++)
			{
				const auto argType = sig.args()[argIdx];

				// have to cast back to explicit register types to gen right mov type
				if (is_general_register(argType))
				{
					cc.mov(args_stack_index, arg_registers.at(argIdx).as<asmjit::x86::Gp>());
				}
				else if (is_XMM_register(argType))
				{
					cc.movq(args_stack_index, arg_registers.at(argIdx).as<asmjit::x86::Xmm>());
				}
				else
				{
					LOG(ERROR) << "Parameters wider than 64bits not supported";
					return 0;
				}

				// next structure slot (+= sizeof(uintptr_t))
				cc.add(i, sizeof(uintptr_t));
			}

			// get pointer to stack structure and pass it to the user pre callback
			asmjit::x86::Gp arg_struct = cc.newUIntPtr("arg_struct");
			cc.lea(arg_struct, m_args_stack);

			// fill reg to pass struct arg count to callback
			asmjit::x86::Gp arg_param_count = cc.newUInt8();
			cc.mov(arg_param_count, (uint8_t)sig.argCount());

			// create buffer for ret val
			asmjit::x86::Mem return_stack = cc.newStack(sizeof(uintptr_t), 16);
			asmjit::x86::Gp return_struct = cc.newUIntPtr("return_struct");
			cc.lea(return_struct, return_stack);

			// fill reg to pass target function pointer to callback
			asmjit::x86::Gp target_func_ptr_reg = cc.newUIntPtr();
			cc.mov(target_func_ptr_reg, target_func_ptr);

			asmjit::Label original_invoke_label      = cc.newLabel();
			asmjit::Label skip_original_invoke_label = cc.newLabel();

			// invoke the user pre callback
			asmjit::InvokeNode* pre_callback_invoke_node;
			cc.invoke(&pre_callback_invoke_node, (uintptr_t)pre_callback, asmjit::FuncSignatureT<bool, parameters_t*, uint8_t, return_value_t*, uintptr_t>());

			// call to user provided function (use ABI of host compiler)
			pre_callback_invoke_node->setArg(0, arg_struct);
			pre_callback_invoke_node->setArg(1, arg_param_count);
			pre_callback_invoke_node->setArg(2, return_struct);
			pre_callback_invoke_node->setArg(3, target_func_ptr_reg);

			// create a register for the user pre callback's return value
			// Note: the size of the register is important for the test instruction. newUInt8 since the pre callback returns a bool.
			asmjit::x86::Gp pre_callback_return_val = cc.newUInt8("pre_callback_return_val");
			// store the callback return value
			pre_callback_invoke_node->setRet(0, pre_callback_return_val);

			// if the callback return value is zero, skip orig.
			cc.test(pre_callback_return_val, pre_callback_return_val);
			cc.jz(skip_original_invoke_label);

			// label to invoke the original function
			cc.bind(original_invoke_label);

			// mov from arguments stack structure into regs
			cc.mov(i, 0); // reset idx
			for (uint8_t arg_idx = 0; arg_idx < sig.argCount(); arg_idx++)
			{
				const auto argType = sig.args()[arg_idx];

				if (is_general_register(argType))
				{
					cc.mov(arg_registers.at(arg_idx).as<asmjit::x86::Gp>(), args_stack_index);
				}
				else if (is_XMM_register(argType))
				{
					cc.movq(arg_registers.at(arg_idx).as<asmjit::x86::Xmm>(), args_stack_index);
				}
				else
				{
					LOG(ERROR) << "Parameters wider than 64bits not supported";
					return 0;
				}

				// next structure slot (+= sizeof(uint64_t))
				cc.add(i, sizeof(uint64_t));
			}

			// deref the trampoline ptr (holder must live longer, must be concrete reg since push later)
			asmjit::x86::Gp original_ptr = cc.zbx();
			cc.mov(original_ptr, m_detour->get_original_ptr());
			cc.mov(original_ptr, asmjit::x86::ptr(original_ptr));

			asmjit::InvokeNode* original_invoke_node;
			cc.invoke(&original_invoke_node, original_ptr, sig);
			for (uint8_t arg_index = 0; arg_index < sig.argCount(); arg_index++)
			{
				original_invoke_node->setArg(arg_index, arg_registers.at(arg_index));
			}

			if (sig.hasRet())
			{
				if (is_general_register(sig.ret()))
				{
					asmjit::x86::Gp tmp = cc.newUIntPtr();
					original_invoke_node->setRet(0, tmp);
					cc.mov(return_stack, tmp);
				}
				else
				{
					asmjit::x86::Xmm tmp = cc.newXmm();
					original_invoke_node->setRet(0, tmp);
					cc.movq(return_stack, tmp);
				}
			}

			cc.bind(skip_original_invoke_label);

			asmjit::InvokeNode* post_callback_invoke_node;
			cc.invoke(&post_callback_invoke_node, (uintptr_t)post_callback, asmjit::FuncSignatureT<void, parameters_t*, uint8_t, return_value_t*, uintptr_t>());

			// Set arguments for the post callback
			post_callback_invoke_node->setArg(0, arg_struct);
			post_callback_invoke_node->setArg(1, arg_param_count);
			post_callback_invoke_node->setArg(2, return_struct);
			post_callback_invoke_node->setArg(3, target_func_ptr_reg);

			if (sig.hasRet())
			{
				asmjit::x86::Mem return_stack_index(return_stack);
				return_stack_index.setSize(sizeof(uintptr_t));
				if (is_general_register(sig.ret()))
				{
					asmjit::x86::Gp tmp2 = cc.newUIntPtr();
					cc.mov(tmp2, return_stack_index);
					cc.ret(tmp2);
				}
				else
				{
					asmjit::x86::Xmm tmp2 = cc.newXmm();
					cc.movq(tmp2, return_stack_index);
					cc.ret(tmp2);
				}
			}

			cc.endFunc();

			// write to buffer
			cc.finalize();

			// worst case, overestimates for case trampolines needed
			code.flatten();
			size_t size = code.codeSize();

			// Allocate a virtual memory (executable).
			m_jit_function_buffer.reserve(size);

			DWORD old_protect;
			VirtualProtect(m_jit_function_buffer.data(), size, PAGE_EXECUTE_READWRITE, &old_protect);

			// if multiple sections, resolve linkage (1 atm)
			if (code.hasUnresolvedLinks())
			{
				code.resolveUnresolvedLinks();
			}

			// Relocate to the base-address of the allocated memory.
			code.relocateToBase((uintptr_t)m_jit_function_buffer.data());
			code.copyFlattenedData(m_jit_function_buffer.data(), size);

			LOG(DEBUG) << "JIT Stub: " << log.data();

			return (uintptr_t)m_jit_function_buffer.data();
		}

		// Construct a callback given the typedef as a string. Types are any valid C/C++ data type (basic types), and pointers to
		// anything are just a uintptr_t. Calling convention is defaulted to whatever is typical for the compiler you use, you can override with
		// stdcall, fastcall, or cdecl (cdecl is default on x86). On x64 those map to the same thing.
		uintptr_t make_jit_func(const std::string& return_type, const std::vector<std::string>& param_types, const asmjit::Arch arch, const user_pre_callback_t pre_callback, const user_post_callback_t post_callback, const uintptr_t target_func_ptr, std::string call_convention = "")
		{
			m_return_type = get_type_info_from_string(return_type);

			asmjit::FuncSignature sig(get_call_convention(call_convention), asmjit::FuncSignature::kNoVarArgs, get_type_id(return_type));

			for (const std::string& s : param_types)
			{
				sig.addArg(get_type_id(s));
				m_param_types.push_back(get_type_info_from_string(s));
			}

			return make_jit_func(sig, arch, pre_callback, post_callback, target_func_ptr);
		}

		void create_and_enable_hook(const std::string& hook_name, uintptr_t target_func_ptr, uintptr_t jitted_func_ptr)
		{
			m_detour->set_instance(hook_name, (void*)target_func_ptr, (void*)jitted_func_ptr);

			m_detour->enable();
		}
	};

	value_wrapper_t::value_wrapper_t(char* val, type_info_t type)
	{
		m_value = val;
		m_type  = type;
	}

	sol::object value_wrapper_t::get(sol::this_state state_)
	{
		if (m_type == type_info_t::boolean_)
		{
			return sol::make_object(big::g_lua_manager->lua_state(), *(bool*)m_value);
		}
		else if (m_type == type_info_t::string_)
		{
			return sol::make_object(big::g_lua_manager->lua_state(), *(const char**)m_value);
		}
		else if (m_type == type_info_t::integer_)
		{
			return sol::make_object(big::g_lua_manager->lua_state(), *(int64_t*)m_value);
		}
		else if (m_type == type_info_t::float_)
		{
			return sol::make_object(big::g_lua_manager->lua_state(), *(float*)m_value);
		}
		else if (m_type == type_info_t::double_)
		{
			return sol::make_object(big::g_lua_manager->lua_state(), *(double*)m_value);
		}
		else
		{
			return sol::nil;
		}
	}

	void value_wrapper_t::set(sol::object new_val, sol::this_state state_)
	{
		if (m_type == type_info_t::boolean_ && new_val.is<bool>())
		{
			*(bool*)m_value = new_val.as<bool>();
		}
		else if (m_type == type_info_t::string_ && new_val.is<const char*>())
		{
			*(const char**)m_value = new_val.as<const char*>();
		}
		else if (m_type == type_info_t::integer_ && new_val.is<int64_t>())
		{
			*(int64_t*)m_value = new_val.as<int64_t>();
		}
		else if (m_type == type_info_t::float_ && new_val.is<float>())
		{
			*(float*)m_value = new_val.as<float>();
		}
		else if (m_type == type_info_t::double_ && new_val.is<double>())
		{
			*(double*)m_value = new_val.as<double>();
		}
	}

	static sol::object to_lua(const runtime_func_t::parameters_t* params, const uint8_t i, const std::vector<type_info_t>& param_types)
	{
		if (param_types[i] == type_info_t::none_)
		{
			return sol::nil;
		}
		else if (param_types[i] == type_info_t::ptr_)
		{
			return sol::make_object(big::g_lua_manager->lua_state(), lua::memory::pointer(params->get<uintptr_t>(i)));
		}
		else
		{
			return sol::make_object(big::g_lua_manager->lua_state(), value_wrapper_t(params->get_arg_ptr(i), param_types[i]));
		}

		return sol::nil;
	}

	static sol::object to_lua(runtime_func_t::return_value_t* return_value, const type_info_t return_value_type)
	{
		if (return_value_type == type_info_t::none_)
		{
			return sol::nil;
		}
		else if (return_value_type == type_info_t::ptr_)
		{
			return sol::make_object(big::g_lua_manager->lua_state(), lua::memory::pointer((uintptr_t)return_value->get()));
		}
		else
		{
			return sol::make_object(big::g_lua_manager->lua_state(), value_wrapper_t((char*)return_value->get(), return_value_type));
		}

		return sol::nil;
	}

	static std::unordered_map<uintptr_t, std::unique_ptr<runtime_func_t>> target_func_ptr_to_hook;

	static bool pre_callback(const runtime_func_t::parameters_t* params, const uint8_t param_count, runtime_func_t::return_value_t* return_value, const uintptr_t target_func_ptr)
	{
		const auto& dyn_hook    = target_func_ptr_to_hook[target_func_ptr];
		const auto& param_types = dyn_hook->m_param_types;
		std::vector<sol::object> args;
		for (uint8_t i = 0; i < param_count; i++)
		{
			args.push_back(to_lua(params, i, param_types));
		}

		return big::g_lua_manager->dynamic_hook_pre_callbacks(target_func_ptr, to_lua(return_value, dyn_hook->m_return_type), args);
	}

	static void post_callback(const runtime_func_t::parameters_t* params, const uint8_t param_count, runtime_func_t::return_value_t* return_value, const uintptr_t target_func_ptr)
	{
		const auto& dyn_hook    = target_func_ptr_to_hook[target_func_ptr];
		const auto& param_types = dyn_hook->m_param_types;
		std::vector<sol::object> args;
		for (uint8_t i = 0; i < param_count; i++)
		{
			args.push_back(to_lua(params, i, param_types));
		}

		big::g_lua_manager->dynamic_hook_post_callbacks(target_func_ptr, to_lua(return_value, dyn_hook->m_return_type), args);
	}

	// Lua API: Function
	// Table: memory
	// Name: dynamic_hook
	// Param: hook_name: string: The name of the hook.
	// Param: return_type: string: Type of the return value of the detoured function.
	// Param: param_types: table<string>: Types of the parameters of the detoured function.
	// Param: target_func_ptr: memory.pointer: The pointer to the function to detour.
	// Param: pre_callback: function: The function that will be called before the original function is about to be called. The callback must match the following signature: ( return_value (value_wrapper), arg1 (value_wrapper), arg2 (value_wrapper), ... ) -> Returns true or false (boolean) depending on whether you want the original function to be called.
	// Param: post_callback: function: The function that will be called after the original function is called (or just after the pre callback is called, if the original function was skipped). The callback must match the following signature: ( return_value (value_wrapper), arg1 (value_wrapper), arg2 (value_wrapper), ... ) -> void
	// **Example Usage:**
	// ```lua
	// local ptr = memory.scan_pattern("some ida sig")
	// memory.dynamic_hook("test_hook", "float", {"const char*"}, ptr,
	// function(ret_val, str)
	//
	//     --str:set("replaced str")
	//     ret_val:set(69.69)
	//     log.info("pre callback from lua", ret_val:get(), str:get())
	//
	//     -- false for skipping the original function call
	//     return false
	// end,
	// function(ret_val, str)
	//     log.info("post callback from lua 1", ret_val:get(), str:get())
	//     ret_val:set(79.69)
	//     log.info("post callback from lua 2", ret_val:get(), str:get())
	// end)
	// ```
	static void dynamic_hook(const std::string& hook_name, const std::string& return_type, sol::table param_types_table, lua::memory::pointer& target_func_ptr_obj, sol::protected_function pre_lua_callback, sol::protected_function post_lua_callback, sol::this_environment env_)
	{
		const auto target_func_ptr = target_func_ptr_obj.get_address();
		if (!target_func_ptr_to_hook.contains(target_func_ptr))
		{
			std::vector<std::string> param_types;
			for (const auto& [k, v] : param_types_table)
			{
				if (v.is<const char*>())
				{
					param_types.push_back(v.as<const char*>());
				}
			}

			std::unique_ptr<runtime_func_t> runtime_func = std::make_unique<runtime_func_t>();
			const auto jitted_func = runtime_func->make_jit_func(return_type, param_types, asmjit::Arch::kHost, pre_callback, post_callback, target_func_ptr);

			target_func_ptr_to_hook.emplace(target_func_ptr, std::move(runtime_func));

			// TODO: The detour_hook is never cleaned up on unload.
			target_func_ptr_to_hook[target_func_ptr]->create_and_enable_hook(hook_name, target_func_ptr, jitted_func);
		}

		if (pre_lua_callback.valid())
		{
			big::lua_module::this_from(env_)->m_data.m_dynamic_hook_pre_callbacks[target_func_ptr].push_back(pre_lua_callback);
		}
		if (post_lua_callback.valid())
		{
			big::lua_module::this_from(env_)->m_data.m_dynamic_hook_post_callbacks[target_func_ptr].push_back(post_lua_callback);
		}
	}

	static std::unordered_map<uintptr_t, std::vector<uint8_t>> jitted_binded_funcs;

	static std::string get_jitted_lua_func_global_name(uintptr_t function_to_call_ptr)
	{
		return std::format("__dynamic_call_{}", function_to_call_ptr);
	}

	class MyErrorHandler : public asmjit::ErrorHandler
	{
	public:
		void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override
		{
			LOG(ERROR) << "asmjit error: " << message;
		}
	};

	static void jit_lua_binded_func(uintptr_t function_to_call_ptr, const asmjit::FuncSignature& function_to_call_sig, const asmjit::Arch& arch, std::vector<type_info_t> param_types, type_info_t return_type, lua_State* lua_state, const std::string& jitted_lua_func_global_name)
	{
		const auto it = jitted_binded_funcs.find(function_to_call_ptr);
		if (it != jitted_binded_funcs.end())
		{
			return;
		}

		asmjit::CodeHolder code;
		auto env = asmjit::Environment::host();
		env.setArch(arch);
		auto asmjit_error = code.init(env);

		// initialize function
		asmjit::x86::Compiler cc(&code);
		// clang-format off
		asmjit::FuncNode* func = cc.addFunc(asmjit::FuncSignature(asmjit::CallConvId::kFastCall, asmjit::FuncSignature::kNoVarArgs,
			asmjit::TypeId::kInt32,
			asmjit::TypeId::kUIntPtr));
		// clang-format on

		asmjit::StringLogger log;
		// clang-format off
			const auto format_flags =
				asmjit::FormatFlags::kMachineCode | asmjit::FormatFlags::kExplainImms | asmjit::FormatFlags::kRegCasts |
				asmjit::FormatFlags::kHexImms     | asmjit::FormatFlags::kHexOffsets  | asmjit::FormatFlags::kPositions;
		// clang-format on

		log.addFlags(format_flags);
		code.setLogger(&log);
		MyErrorHandler myErrorHandler;
		code.setErrorHandler(&myErrorHandler);

		// too small to really need it
		func->frame().resetPreservedFP();

		// map argument slots to registers, following abi.
		std::vector<asmjit::x86::Reg> arg_registers;
		for (uint8_t arg_index = 0; arg_index < function_to_call_sig.argCount(); arg_index++)
		{
			const auto arg_type = function_to_call_sig.args()[arg_index];

			// for each "function to call" parameter
			// InvokeNode for the corresponding lua_toXXX func
			// result of the invokenode setRet goes to the arg Reg.
			// those Reg will then be setArg of the function_to_call final InvokeNode.

			asmjit::x86::Reg arg;

			const auto arg_type_info = param_types[arg_index];
			if (arg_type_info == type_info_t::integer_ || arg_type_info == type_info_t::float_ || arg_type_info == type_info_t::double_)
			{
				asmjit::InvokeNode* lua_tofunc;
				// clang-format off
				cc.invoke(&lua_tofunc, lua_tonumberx, asmjit::FuncSignature(asmjit::CallConvId::kCDecl, asmjit::FuncSignature::kNoVarArgs,
					asmjit::TypeId::kFloat64,
					asmjit::TypeId::kUIntPtr, asmjit::TypeId::kInt32, asmjit::TypeId::kUIntPtr));
				// clang-format on

				lua_tofunc->setArg(0, lua_state);
				lua_tofunc->setArg(1, arg_index + 1);
				lua_tofunc->setArg(2, NULL);

				const auto tmp = cc.newXmm();
				lua_tofunc->setRet(0, tmp);
				if (arg_type_info == type_info_t::integer_)
				{
					arg = cc.newUIntPtr();
					cc.cvttsd2si(arg.as<asmjit::x86::Gp>(), tmp);
				}
				else if (arg_type_info == type_info_t::float_)
				{
					arg = cc.newXmm();
					cc.cvtsd2ss(arg.as<asmjit::x86::Xmm>(), tmp);
				}
				else
				{
					arg = tmp;
				}
			}
			else if (arg_type_info == type_info_t::boolean_)
			{
				asmjit::InvokeNode* lua_tofunc;
				// clang-format off
				cc.invoke(&lua_tofunc, lua_toboolean, asmjit::FuncSignature(asmjit::CallConvId::kCDecl, asmjit::FuncSignature::kNoVarArgs,
					asmjit::TypeId::kInt32,
					asmjit::TypeId::kUIntPtr, asmjit::TypeId::kInt32));
				// clang-format on

				lua_tofunc->setArg(0, lua_state);
				lua_tofunc->setArg(1, arg_index + 1);

				arg = cc.newUIntPtr();
				lua_tofunc->setRet(0, arg);
			}
			else if (arg_type_info == type_info_t::string_)
			{
				asmjit::InvokeNode* lua_tofunc;
				// clang-format off
				cc.invoke(&lua_tofunc, lua_tolstring, asmjit::FuncSignature(asmjit::CallConvId::kCDecl, asmjit::FuncSignature::kNoVarArgs,
					asmjit::TypeId::kUIntPtr,
					asmjit::TypeId::kUIntPtr, asmjit::TypeId::kInt32, asmjit::TypeId::kUIntPtr));
				// clang-format on

				lua_tofunc->setArg(0, lua_state);
				lua_tofunc->setArg(1, arg_index + 1);
				lua_tofunc->setArg(2, NULL);

				arg = cc.newUIntPtr();
				lua_tofunc->setRet(0, arg);
			}
			else if (arg_type_info == type_info_t::ptr_)
			{
				asmjit::InvokeNode* lua_tofunc;
				// clang-format off
				cc.invoke(&lua_tofunc, lua_tonumberx, asmjit::FuncSignature(asmjit::CallConvId::kCDecl, asmjit::FuncSignature::kNoVarArgs,
					asmjit::TypeId::kFloat64,
					asmjit::TypeId::kUIntPtr, asmjit::TypeId::kInt32, asmjit::TypeId::kUIntPtr));
				// clang-format on

				lua_tofunc->setArg(0, lua_state);
				lua_tofunc->setArg(1, arg_index + 1);
				lua_tofunc->setArg(2, NULL);

				// lua_Number (double) to integer type
				const auto tmp = cc.newXmm();
				lua_tofunc->setRet(0, tmp);
				arg = cc.newUIntPtr();
				cc.cvttsd2si(arg.as<asmjit::x86::Gp>(), tmp);
			}

			arg_registers.push_back(arg);
		}

		asmjit::InvokeNode* function_to_call_invoke_node;
		cc.invoke(&function_to_call_invoke_node, function_to_call_ptr, function_to_call_sig);
		for (uint8_t arg_index = 0; arg_index < function_to_call_sig.argCount(); arg_index++)
		{
			function_to_call_invoke_node->setArg(arg_index, arg_registers.at(arg_index));
		}
		asmjit::x86::Reg function_to_call_return_val_reg;
		if (is_general_register(function_to_call_sig.ret()))
		{
			function_to_call_return_val_reg = cc.newUIntPtr();
		}
		else if (is_XMM_register(function_to_call_sig.ret()))
		{
			function_to_call_return_val_reg = cc.newXmm();
		}
		else
		{
			LOG(ERROR) << "Return val wider than 64bits not supported";
			return;
		}
		function_to_call_invoke_node->setRet(0, function_to_call_return_val_reg);

		if (return_type == type_info_t::integer_ || return_type == type_info_t::float_ || return_type == type_info_t::double_)
		{
			if (function_to_call_sig.ret() >= asmjit::TypeId::_kIntStart && function_to_call_sig.ret() <= asmjit::TypeId::_kIntEnd)
			{
				// the function returned to a Gp register, need to convert it to a lua_Number compatible register.
				const auto tmp = cc.newXmm();
				cc.cvtsi2sd(tmp, function_to_call_return_val_reg.as<asmjit::x86::Gp>());
				function_to_call_return_val_reg = tmp;
			}
			else if (function_to_call_sig.ret() == asmjit::TypeId::kFloat32)
			{
				// m128_f32 -> m128_f64 (lua_Number)
				cc.cvtss2sd(function_to_call_return_val_reg.as<asmjit::x86::Xmm>(), function_to_call_return_val_reg.as<asmjit::x86::Xmm>());
			}

			asmjit::InvokeNode* lua_pushfunc;
			// clang-format off
			cc.invoke(&lua_pushfunc, lua_pushnumber, asmjit::FuncSignature(asmjit::CallConvId::kCDecl, asmjit::FuncSignature::kNoVarArgs,
				asmjit::TypeId::kVoid,
				asmjit::TypeId::kUIntPtr, asmjit::TypeId::kFloat64));
			// clang-format on

			lua_pushfunc->setArg(0, lua_state);
			lua_pushfunc->setArg(1, function_to_call_return_val_reg);
		}
		else if (return_type == type_info_t::boolean_)
		{
			asmjit::InvokeNode* lua_pushfunc;
			// clang-format off
			cc.invoke(&lua_pushfunc, lua_pushboolean, asmjit::FuncSignature(asmjit::CallConvId::kCDecl, asmjit::FuncSignature::kNoVarArgs,
				asmjit::TypeId::kVoid,
				asmjit::TypeId::kUIntPtr, asmjit::TypeId::kInt8));
			// clang-format on

			lua_pushfunc->setArg(0, lua_state);
			lua_pushfunc->setArg(1, function_to_call_return_val_reg);
		}
		else if (return_type == type_info_t::string_)
		{
			asmjit::InvokeNode* lua_pushfunc;
			// clang-format off
			cc.invoke(&lua_pushfunc, lua_pushstring, asmjit::FuncSignature(asmjit::CallConvId::kCDecl, asmjit::FuncSignature::kNoVarArgs,
				asmjit::TypeId::kVoid,
				asmjit::TypeId::kUIntPtr, asmjit::TypeId::kUIntPtr));
			// clang-format on

			lua_pushfunc->setArg(0, lua_state);
			lua_pushfunc->setArg(1, function_to_call_return_val_reg);
		}
		else if (return_type == type_info_t::ptr_)
		{
			// integer type to lua_Number (double)
			asmjit::x86::Xmm tmp = cc.newXmm();
			cc.cvtsi2sd(tmp, function_to_call_return_val_reg.as<asmjit::x86::Gp>());

			asmjit::InvokeNode* lua_pushfunc;
			// clang-format off
			cc.invoke(&lua_pushfunc, lua_pushnumber, asmjit::FuncSignature(asmjit::CallConvId::kCDecl, asmjit::FuncSignature::kNoVarArgs,
				asmjit::TypeId::kVoid,
				asmjit::TypeId::kUIntPtr, asmjit::TypeId::kFloat64));
			// clang-format on

			lua_pushfunc->setArg(0, lua_state);
			lua_pushfunc->setArg(1, tmp);
		}

		// a lua binded c func always return an int which hold the number of returned vars.
		asmjit::x86::Gp retReg = cc.newUIntPtr();
		cc.mov(retReg, function_to_call_sig.hasRet() ? 1 : 0);
		cc.ret(retReg);

		cc.endFunc();

		// write to buffer
		cc.finalize();

		// worst case, overestimates for case trampolines needed
		code.flatten();
		size_t size = code.codeSize();

		// Allocate a virtual memory (executable).
		static std::vector<uint8_t> jit_function_buffer(size);
		DWORD old_protect;
		VirtualProtect(jit_function_buffer.data(), size, PAGE_EXECUTE_READWRITE, &old_protect);

		// if multiple sections, resolve linkage (1 atm)
		if (code.hasUnresolvedLinks())
		{
			code.resolveUnresolvedLinks();
		}

		// Relocate to the base-address of the allocated memory.
		code.relocateToBase((uintptr_t)jit_function_buffer.data());
		code.copyFlattenedData(jit_function_buffer.data(), size);

		LOG(DEBUG) << "JIT Stub: " << log.data();

		lua_pushcfunction(lua_state, (lua_CFunction)jit_function_buffer.data());
		lua_setglobal(lua_state, jitted_lua_func_global_name.c_str());
	}

	// Lua API: Function
	// Table: memory
	// Name: dynamic_call
	// Param: return_type: string: Type of the return value of the function to call.
	// Param: param_types: table<string>: Types of the parameters of the function to call.
	// Param: target_func_ptr: memory.pointer: The pointer to the function to call.
	// **Example Usage:**
	// ```lua
	// local ptr = memory.scan_pattern("some ida sig")
	// local func_to_call_test_global_name = memory.dynamic_call("bool", {"const char*", "float", "double", "void*", "int8_t", "int64_t"}, ptr)
	// local call_res_test = _G[func_to_call_test_global_name]("yepi", 69.025, 420.69, 57005, 126, 1195861093)
	// log.info("call_res_test: ", call_res_test)
	// ```
	static std::string dynamic_call(const std::string& return_type, sol::table param_types_table, lua::memory::pointer& target_func_ptr_obj, sol::this_environment env_)
	{
		const auto target_func_ptr = target_func_ptr_obj.get_address();

		const auto jitted_lua_func_global_name = get_jitted_lua_func_global_name(target_func_ptr);

		const auto already_jitted_func = env_.env.value()[jitted_lua_func_global_name];
		if (already_jitted_func.is<sol::protected_function>())
		{
			return already_jitted_func;
		}

		std::vector<std::string> param_types_strings;
		for (const auto& [k, v] : param_types_table)
		{
			if (v.is<const char*>())
			{
				param_types_strings.push_back(v.as<const char*>());
			}
		}

		std::string call_convention = "";
		asmjit::FuncSignature sig(get_call_convention(call_convention), asmjit::FuncSignature::kNoVarArgs, get_type_id(return_type));

		std::vector<type_info_t> param_types;
		for (const std::string& s : param_types_strings)
		{
			sig.addArg(get_type_id(s));
			param_types.push_back(get_type_info_from_string(s));
		}

		jit_lua_binded_func(target_func_ptr,
		                    sig,
		                    asmjit::Arch::kHost,
		                    param_types,
		                    get_type_info_from_string(return_type),
		                    env_.env.value().lua_state(),
		                    jitted_lua_func_global_name);

		return jitted_lua_func_global_name;
	}

	void bind(sol::state_view& state)
	{
		auto ns = state["memory"].get_or_create<sol::table>();

		auto pointer_ut           = ns.new_usertype<pointer>("pointer", sol::constructors<pointer(uintptr_t)>());
		pointer_ut["add"]         = &pointer::add;
		pointer_ut["sub"]         = &pointer::sub;
		pointer_ut["rip"]         = &pointer::rip;
		pointer_ut["rip_cmp"]     = &pointer::rip_cmp;
		pointer_ut["get_byte"]    = &pointer::get<uint8_t>;
		pointer_ut["get_word"]    = &pointer::get<uint16_t>;
		pointer_ut["get_dword"]   = &pointer::get<uint32_t>;
		pointer_ut["get_qword"]   = &pointer::get<uint64_t>;
		pointer_ut["get_float"]   = &pointer::get<float>;
		pointer_ut["get_string"]  = &pointer::get_string;
		pointer_ut["set_byte"]    = &pointer::set<uint8_t>;
		pointer_ut["set_word"]    = &pointer::set<uint16_t>;
		pointer_ut["set_dword"]   = &pointer::set<uint32_t>;
		pointer_ut["set_qword"]   = &pointer::set<uint64_t>;
		pointer_ut["set_float"]   = &pointer::set<float>;
		pointer_ut["set_string"]  = &pointer::set_string;
		pointer_ut["patch_byte"]  = &pointer::patch<uint8_t>;
		pointer_ut["patch_word"]  = &pointer::patch<uint16_t>;
		pointer_ut["patch_dword"] = &pointer::patch<uint32_t>;
		pointer_ut["patch_qword"] = &pointer::patch<uint64_t>;
		pointer_ut["is_null"]     = &pointer::is_null;
		pointer_ut["is_valid"]    = &pointer::is_valid;
		pointer_ut["deref"]       = &pointer::deref;
		pointer_ut["get_address"] = &pointer::get_address;

		auto patch_ut       = ns.new_usertype<big::lua_patch>("patch", sol::no_constructor);
		patch_ut["apply"]   = &big::lua_patch::apply;
		patch_ut["restore"] = &big::lua_patch::restore;

		ns["scan_pattern"] = scan_pattern;
		ns["allocate"]     = allocate;
		ns["free"]         = free;

		ns.new_usertype<value_wrapper_t>("value_wrapper", "get", &value_wrapper_t::get, "set", &value_wrapper_t::set);
		ns["dynamic_hook"] = dynamic_hook;

		ns["dynamic_call"] = dynamic_call;
	}
} // namespace lua::memory
