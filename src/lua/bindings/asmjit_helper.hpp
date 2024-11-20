#pragma once
#include <asmjit/asmjit.h>
#include <optional>
#include <string>

namespace lua::memory
{
	// does a given type fit in a general purpose register (i.e. is it integer type)
	bool is_general_register(const asmjit::TypeId type_id);

	// float, double, simd128
	bool is_XMM_register(const asmjit::TypeId type_id);

	asmjit::CallConvId get_call_convention(const std::string& conv);

	asmjit::TypeId get_type_id(const std::string& type);

	std::optional<asmjit::x86::Gp> get_gp_from_name(const std::string& name);

	std::optional<asmjit::x86::Xmm> get_XMM_from_name(const std::string& name);

	std::optional<asmjit::x86::Mem> get_addr_from_name(const std::string& name, const int64_t rsp_offset = 0);
} // namespace lua::memory
