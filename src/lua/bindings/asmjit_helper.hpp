#pragma once
#include <asmjit/asmjit.h>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <string/string.hpp>

namespace lua::memory
{
	// does a given type fit in a general purpose register (i.e. is it integer type)
	bool is_general_register(const asmjit::TypeId type_id);

	// float, double, simd128
	bool is_XMM_register(const asmjit::TypeId type_id);

	asmjit::CallConvId get_call_convention(const std::string& conv);

	asmjit::TypeId get_type_id(const std::string& type);

	std::optional<asmjit::x86::Gp> get_gp_from_name(const std::string& name);

	std::optional<asmjit::x86::Xmm> get_xmm_from_name(const std::string& name);

	std::string parse_address_component(std::string_view name, size_t& index);

	std::optional<uint64_t> parse_number_from_string(std::string_view str);

	std::optional<asmjit::x86::Mem> get_addr_from_name(std::string_view name, const int64_t rsp_offset = 0);

	std::vector<uint32_t> get_useable_gp_id_from_name(std::string_view name);
} // namespace lua::memory
