#pragma once
#include "runtime_func_t.hpp"

#include "lua/lua_manager.hpp"

// clang-format off
#include <AsyncLogger/Logger.hpp>
using namespace al;

// clang-format on

namespace lua::memory
{
	char* runtime_func_t::parameters_t::get_arg_ptr(const uint8_t idx) const
	{
		return ((char*)&m_arguments) + sizeof(uintptr_t) * idx;
	}

	unsigned char* runtime_func_t::return_value_t::get() const
	{
		return (unsigned char*)&m_return_value;
	}

	runtime_func_t::runtime_func_t()
	{
		m_detour      = std::make_unique<big::detour_hook>();
		m_return_type = {type_info_t::none_};
	}

	runtime_func_t::~runtime_func_t()
	{
		big::g_lua_manager->m_target_func_ptr_to_dynamic_hook.erase(m_target_func_ptr);

		m_detour->disable();
	}

	void runtime_func_t::debug_print_args(const asmjit::FuncSignature& sig)
	{
		for (uint8_t arg_index_debug = 0; arg_index_debug < sig.argCount(); arg_index_debug++)
		{
			const auto arg_type_debug = sig.args()[arg_index_debug];

			LOG(DEBUG) << (int)arg_type_debug;
		}
	}

	uintptr_t runtime_func_t::make_jit_func(const asmjit::FuncSignature& sig, const asmjit::Arch arch, const user_pre_callback_t pre_callback, const user_post_callback_t post_callback, const uintptr_t target_func_ptr)
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
				LOG(ERROR) << "Parameters wider than 64bits not supported, index: " << arg_index << " | " << (int)arg_type;
				debug_print_args(sig);

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
				LOG(ERROR) << "Parameters wider than 64bits not supported, index: " << argIdx << " | " << (int)argType;
				debug_print_args(sig);

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
				LOG(ERROR) << "Parameters wider than 64bits not supported, index: " << arg_idx << " | " << (int)argType;
				debug_print_args(sig);

				return 0;
			}

			// next structure slot (+= sizeof(uint64_t))
			cc.add(i, sizeof(uint64_t));
		}

		// deref the trampoline ptr (holder must live longer, must be concrete reg since push later)
		asmjit::x86::Gp original_ptr = cc.newUIntPtr();
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

	uintptr_t runtime_func_t::make_jit_func(const std::string& return_type, const std::vector<std::string>& param_types, const asmjit::Arch arch, const user_pre_callback_t pre_callback, const user_post_callback_t post_callback, const uintptr_t target_func_ptr, std::string call_convention)
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

	class jit_error_handler : public asmjit::ErrorHandler
	{
	public:
		jit_error_handler()
		{
		}

		void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override
		{
			LOG(ERROR) << message;
		}
	};

	uintptr_t runtime_func_t::make_jit_midfunc(const std::vector<std::string>& param_types, const std::vector<std::string>& param_captures, const int stack_restore_offset, const asmjit::Arch arch, mid_callback_t mid_callback, const uintptr_t target_func_ptr)
	{
		for (const std::string& s : param_types)
		{
			m_param_types.push_back(get_type_info_from_string(s));
		}

		asmjit::CodeHolder code;
		auto env = asmjit::Environment::host();
		env.setArch(arch);
		code.init(env);
		jit_error_handler eh;
		code.setErrorHandler(&eh);

		// initialize function
		asmjit::x86::Assembler cc(&code);

		asmjit::StringLogger log;
		// clang-format off
			const auto format_flags =
				asmjit::FormatFlags::kMachineCode | asmjit::FormatFlags::kExplainImms | asmjit::FormatFlags::kRegCasts |
				asmjit::FormatFlags::kHexImms     | asmjit::FormatFlags::kHexOffsets  | asmjit::FormatFlags::kPositions;
		// clang-format on

		log.addFlags(format_flags);
		code.setLogger(&log);

		asmjit::Label original_invoke_label = cc.newLabel();

		// save caller-saved registers
		cc.push(asmjit::x86::qword_ptr((uint64_t)m_detour->get_original_ptr()));
		cc.push(asmjit::x86::rbp);
		cc.push(asmjit::x86::rax);
		cc.push(asmjit::x86::rcx);
		cc.push(asmjit::x86::rdx);
		cc.push(asmjit::x86::r8);
		cc.push(asmjit::x86::r9);
		cc.push(asmjit::x86::r10);
		cc.push(asmjit::x86::r11);

		// setup the stack structure to hold arguments for user callback
		int32_t stack_size = 16 * param_types.size();

		// allocate space
		cc.sub(asmjit::x86::rsp, stack_size);

		// save capture registers to save change
		std::unordered_map<uint8_t, asmjit::x86::Gp> cap_Gps;

		// capture registers to the stack
		for (uint8_t argIdx = 0; argIdx < param_types.size(); argIdx++)
		{
			auto argType    = get_type_id(param_types.at(argIdx));
			auto argCapture = param_captures.at(argIdx);
			if (argCapture.at(0) == '[')
			{
				if (is_general_register(argType))
				{
					// caller-saved registers' offset + temp register's offset
					auto target_address = get_addr_from_name(argCapture, stack_size + 8 * 9 + 8);
					if (!target_address.has_value())
					{
						LOG(ERROR) << "Can't get address from the name";
						return 0;
					}
					cc.push(asmjit::x86::rbp);
					cc.mov(asmjit::x86::rbp, *target_address);
					cc.mov(asmjit::x86::ptr(asmjit::x86::rsp, 16 * argIdx + 8), asmjit::x86::rbp);
					cc.pop(asmjit::x86::rbp);
				}
				else if (is_XMM_register(argType))
				{
					auto target_address = get_addr_from_name(argCapture, stack_size + 8 * 9 + 16);
					if (!target_address.has_value())
					{
						LOG(ERROR) << "Can't get address from the name";
						return 0;
					}
					cc.sub(asmjit::x86::rsp, 16);
					cc.movq(asmjit::x86::ptr(asmjit::x86::rsp, 16), asmjit::x86::xmm0);
					cc.movq(asmjit::x86::xmm0, *target_address);
					cc.movq(asmjit::x86::ptr(asmjit::x86::rsp, 16 * argIdx + 16), asmjit::x86::xmm0);
					cc.movq(asmjit::x86::xmm0, asmjit::x86::ptr(asmjit::x86::rsp, 16));
					cc.add(asmjit::x86::rsp, 16);
				}
				else
				{
					LOG(ERROR) << "Parameters wider than 64bits not supported";
					return 0;
				}
			}
			else
			{
				if (is_general_register(argType))
				{
					auto target_reg = get_gp_from_name(argCapture);
					if (!target_reg.has_value())
					{
						auto target_address = get_addr_from_name('[' + argCapture + ']', stack_size + 8 * 9 + 8);
						if (!target_address.has_value())
						{
							LOG(ERROR) << "Can't get register from the name";
							return 0;
						}
						cc.push(asmjit::x86::rbp);
						cc.lea(asmjit::x86::rbp, *target_address);
						cc.mov(asmjit::x86::ptr(asmjit::x86::rsp, 16 * argIdx + 8), asmjit::x86::rbp);
						cc.pop(asmjit::x86::rbp);
					}
					else
					{
						cc.mov(asmjit::x86::ptr(asmjit::x86::rsp, 16 * argIdx), *target_reg);
						cap_Gps[argIdx] = *target_reg;
					}
				}
				else if (is_XMM_register(argType))
				{
					auto target_reg = get_XMM_from_name(argCapture);
					if (!target_reg.has_value())
					{
						LOG(ERROR) << "Can't get register from the name";
						return 0;
					}
					cc.movq(asmjit::x86::ptr(asmjit::x86::rsp, 16 * argIdx), *target_reg);
				}
				else
				{
					LOG(ERROR) << "Parameters wider than 64bits not supported";
					return 0;
				}
			}
		}

		// pass arguments to the function
		cc.mov(asmjit::x86::rcx, asmjit::x86::rsp);
		cc.mov(asmjit::x86::rdx, param_types.size());
		cc.mov(asmjit::x86::r8, (uintptr_t)target_func_ptr);

		// save the rsp
		cc.mov(asmjit::x86::rbp, asmjit::x86::rsp);

		// allocate prelogue space, may require a bigger space
		cc.sub(asmjit::x86::rsp, 128);

		// stack alignment
		cc.and_(asmjit::x86::rsp, -16);

		// invoke the mid callback
		cc.mov(asmjit::x86::r9, (uintptr_t)mid_callback);
		cc.call(asmjit::x86::r9);

		// restore rsp
		cc.mov(asmjit::x86::rsp, asmjit::x86::rbp);

		// if the callback return value is zero, skip orig.
		cc.test(asmjit::x86::rax, asmjit::x86::rax);
		cc.jz(original_invoke_label);
		cc.mov(asmjit::x86::ptr(asmjit::x86::rsp, stack_size + 8 * 8), asmjit::x86::rax);
		cc.bind(original_invoke_label);
		// apply change
		for (uint8_t argIdx = 0; argIdx < param_types.size(); argIdx++)
		{
			auto argType    = get_type_id(param_types.at(argIdx));
			auto argCapture = param_captures.at(argIdx);
			if (argCapture.at(0) == '[')
			{
				if (is_general_register(argType))
				{
					// caller-saved registers' offset + temp register's offset
					auto target_address = get_addr_from_name(argCapture, stack_size + 8 * 9 + 8);
					if (!target_address.has_value())
					{
						LOG(ERROR) << "Can't get address from the name";
						return 0;
					}
					cc.push(asmjit::x86::rbp);
					cc.mov(asmjit::x86::rbp, asmjit::x86::ptr(asmjit::x86::rsp, 16 * argIdx + 8));
					cc.mov(*target_address, asmjit::x86::rbp);
					cc.pop(asmjit::x86::rbp);
				}
				else if (is_XMM_register(argType))
				{
					auto target_address = get_addr_from_name(argCapture, stack_size + 8 * 9 + 16);
					if (!target_address.has_value())
					{
						LOG(ERROR) << "Can't get address from the name";
						return 0;
					}
					cc.sub(asmjit::x86::rsp, 16);
					cc.movq(asmjit::x86::ptr(asmjit::x86::rsp, 16), asmjit::x86::xmm0);
					cc.movq(asmjit::x86::xmm0, asmjit::x86::ptr(asmjit::x86::rsp, 16 * argIdx + 16));
					cc.movq(*target_address, asmjit::x86::xmm0);
					cc.movq(asmjit::x86::xmm0, asmjit::x86::ptr(asmjit::x86::rsp, 16));
					cc.add(asmjit::x86::rsp, 16);
				}
				else
				{
					LOG(ERROR) << "Parameters wider than 64bits not supported";
					return 0;
				}
			}
			else
			{
				if (is_general_register(argType))
				{
					auto target_reg = get_gp_from_name(argCapture);
					if (!target_reg.has_value())
					{
						auto target_address = get_addr_from_name('[' + argCapture + ']', stack_size + 8 * 9 + 8);
						if (!target_address.has_value())
						{
							LOG(ERROR) << "Can't get register from the name";
							return 0;
						}
						cc.push(asmjit::x86::rbp);
						cc.lea(asmjit::x86::rbp, asmjit::x86::ptr(asmjit::x86::rsp, 16 * argIdx + 8));
						cc.mov(*target_address, asmjit::x86::rbp);
						cc.pop(asmjit::x86::rbp);
					}
					else
					{
						cc.mov(*target_reg, asmjit::x86::ptr(asmjit::x86::rsp, 16 * argIdx));
					}
				}
				else if (is_XMM_register(argType))
				{
					auto target_reg = get_XMM_from_name(argCapture);
					if (!target_reg.has_value())
					{
						LOG(ERROR) << "Can't get register from the name";
						return 0;
					}
					cc.movq(*target_reg, asmjit::x86::ptr(asmjit::x86::rsp, 16 * argIdx));
				}
				else
				{
					LOG(ERROR) << "Parameters wider than 64bits not supported";
					return 0;
				}
			}
		}

		// stack cleanup
		cc.add(asmjit::x86::rsp, stack_size);

		// skip capture registers
		auto change_pop = [&](asmjit::x86::Gp reg)
		{
			for (const auto& pair : cap_Gps)
			{
				if (pair.second == reg)
				{
					cc.add(asmjit::x86::rsp, 8);
					return;
				}
			}
			cc.pop(reg);
		};
		// restore caller-saved registers
		change_pop(asmjit::x86::r11);
		change_pop(asmjit::x86::r10);
		change_pop(asmjit::x86::r9);
		change_pop(asmjit::x86::r8);
		change_pop(asmjit::x86::rdx);
		change_pop(asmjit::x86::rcx);
		change_pop(asmjit::x86::rax);
		cc.pop(asmjit::x86::rbp);

		if (stack_restore_offset != 0)
		{
			cc.sub(asmjit::x86::rsp, stack_restore_offset);
		}

		// jump to the original function
		cc.ret();

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

	void runtime_func_t::create_and_enable_hook(const std::string& hook_name, uintptr_t target_func_ptr, uintptr_t jitted_func_ptr, bool is_follow_call_on_fn_address)
	{
		m_target_func_ptr = target_func_ptr;

		m_detour->set_instance(hook_name, (void*)target_func_ptr, (void*)jitted_func_ptr);

		m_detour->set_is_follow_call_on_fn_address(is_follow_call_on_fn_address);

		m_detour->enable();
	}
} // namespace lua::memory
