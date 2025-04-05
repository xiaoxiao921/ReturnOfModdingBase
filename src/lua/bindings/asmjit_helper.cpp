#pragma once
#include "asmjit_helper.hpp"

#include <ankerl/unordered_dense.h>
#include <charconv>

// clang-format off
#include <AsyncLogger/Logger.hpp>
using namespace al;

// clang-format on

namespace lua::memory
{
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

	std::optional<asmjit::x86::Gp> get_gp_from_name(const std::string& name)
	{
		// clang-format off
		static const ankerl::unordered_dense::map<std::string, asmjit::x86::Gp> reg_map =
		{
			// 64-bit registers
			{"rax", asmjit::x86::rax}, // RAX: 64-bit
			{"rbx", asmjit::x86::rbx}, // RBX: 64-bit
			{"rcx", asmjit::x86::rcx}, // RCX: 64-bit
			{"rdx", asmjit::x86::rdx}, // RDX: 64-bit
			{"rsi", asmjit::x86::rsi}, // RSI: 64-bit
			{"rdi", asmjit::x86::rdi}, // RDI: 64-bit
			{"rbp", asmjit::x86::rbp}, // RBP: 64-bit
			{"rsp", asmjit::x86::rsp}, // RSP: 64-bit
			{"r8", asmjit::x86::r8},   // R8: 64-bit
			{"r9", asmjit::x86::r9},   // R9: 64-bit
			{"r10", asmjit::x86::r10}, // R10: 64-bit
			{"r11", asmjit::x86::r11}, // R11: 64-bit
			{"r12", asmjit::x86::r12}, // R12: 64-bit
			{"r13", asmjit::x86::r13}, // R13: 64-bit
			{"r14", asmjit::x86::r14}, // R14: 64-bit
			{"r15", asmjit::x86::r15}, // R15: 64-bit

			// 32-bit registers (lower 32 bits of 64-bit registers)
			{"eax", asmjit::x86::eax},   // EAX: low 32-bits of RAX
			{"ebx", asmjit::x86::ebx},   // EBX: low 32-bits of RBX
			{"ecx", asmjit::x86::ecx},   // ECX: low 32-bits of RCX
			{"edx", asmjit::x86::edx},   // EDX: low 32-bits of RDX
			{"esi", asmjit::x86::esi},   // ESI: low 32-bits of RSI
			{"edi", asmjit::x86::edi},   // EDI: low 32-bits of RDI
			{"ebp", asmjit::x86::ebp},   // EBP: low 32-bits of RBP
			{"esp", asmjit::x86::esp},   // ESP: low 32-bits of RSP
			{"r8d", asmjit::x86::r8d},   // R8D: low 32-bits of R8
			{"r9d", asmjit::x86::r9d},   // R9D: low 32-bits of R9
			{"r10d", asmjit::x86::r10d}, // R10D: low 32-bits of R10
			{"r11d", asmjit::x86::r11d}, // R11D: low 32-bits of R11
			{"r12d", asmjit::x86::r12d}, // R12D: low 32-bits of R12
			{"r13d", asmjit::x86::r13d}, // R13D: low 32-bits of R13
			{"r14d", asmjit::x86::r14d}, // R14D: low 32-bits of R14
			{"r15d", asmjit::x86::r15d}, // R15D: low 32-bits of R15

			// 16-bit registers (lower 16 bits of 64-bit registers)
			{"ax", asmjit::x86::ax},     // AX: low 16-bits of RAX
			{"bx", asmjit::x86::bx},     // BX: low 16-bits of RBX
			{"cx", asmjit::x86::cx},     // CX: low 16-bits of RCX
			{"dx", asmjit::x86::dx},     // DX: low 16-bits of RDX
			{"si", asmjit::x86::si},     // SI: low 16-bits of RSI
			{"di", asmjit::x86::di},     // DI: low 16-bits of RDI
			{"bp", asmjit::x86::bp},     // BP: low 16-bits of RBP
			{"sp", asmjit::x86::sp},     // SP: low 16-bits of RSP
			{"r8w", asmjit::x86::r8w},   // R8W: low 16-bits of R8
			{"r9w", asmjit::x86::r9w},   // R9W: low 16-bits of R9
			{"r10w", asmjit::x86::r10w}, // R10W: low 16-bits of R10
			{"r11w", asmjit::x86::r11w}, // R11W: low 16-bits of R11
			{"r12w", asmjit::x86::r12w}, // R12W: low 16-bits of R12
			{"r13w", asmjit::x86::r13w}, // R13W: low 16-bits of R13
			{"r14w", asmjit::x86::r14w}, // R14W: low 16-bits of R14
			{"r15w", asmjit::x86::r15w}, // R15W: low 16-bits of R15

			// 8-bit registers (lower 8 bits of 64-bit registers)
			{"al", asmjit::x86::al},     // AL: low 8-bits of RAX
			{"ah", asmjit::x86::ah},     // AH: high 8-bits of RAX
			{"bl", asmjit::x86::bl},     // BL: low 8-bits of RBX
			{"bh", asmjit::x86::bh},     // BH: high 8-bits of RBX
			{"cl", asmjit::x86::cl},     // CL: low 8-bits of RCX
			{"ch", asmjit::x86::ch},     // CH: high 8-bits of RCX
			{"dl", asmjit::x86::dl},     // DL: low 8-bits of RDX
			{"dh", asmjit::x86::dh},     // DH: high 8-bits of RDX
			{"sil", asmjit::x86::sil},   // SIL: low 8-bits of RSI
			{"dil", asmjit::x86::dil},   // DIL: low 8-bits of RDI
			{"bpl", asmjit::x86::bpl},   // BPL: low 8-bits of RBP
			{"spl", asmjit::x86::spl},    // SPL: low 8-bits of RSP
			{"r8b", asmjit::x86::r8b},   // R8B: low 8-bits of R8
			{"r9b", asmjit::x86::r9b},   // R9B: low 8-bits of R9
			{"r10b", asmjit::x86::r10b}, // R10B: low 8-bits of R10
			{"r11b", asmjit::x86::r11b}, // R11B: low 8-bits of R11
			{"r12b", asmjit::x86::r12b}, // R12B: low 8-bits of R12
			{"r13b", asmjit::x86::r13b}, // R13B: low 8-bits of R13
			{"r14b", asmjit::x86::r14b}, // R14B: low 8-bits of R14
			{"r15b", asmjit::x86::r15b} // R15B: low 8-bits of R15
		};
		// clang-format on

		const auto it = reg_map.find(name);
		if (it != reg_map.end())
		{
			return it->second;
		}
		else
		{
			return std::nullopt;
		}
	}

	std::optional<asmjit::x86::Xmm> get_xmm_from_name(const std::string& name)
	{
		// clang-format off
		static const ankerl::unordered_dense::map<std::string, asmjit::x86::Xmm> reg_map =
		{
			{"xmm0", asmjit::x86::xmm0},   {"xmm1", asmjit::x86::xmm1},   {"xmm2", asmjit::x86::xmm2},
			{"xmm3", asmjit::x86::xmm3},   {"xmm4", asmjit::x86::xmm4},   {"xmm5", asmjit::x86::xmm5},
			{"xmm6", asmjit::x86::xmm6},   {"xmm7", asmjit::x86::xmm7},   {"xmm8", asmjit::x86::xmm8},
			{"xmm9", asmjit::x86::xmm9},   {"xmm10", asmjit::x86::xmm10}, {"xmm11", asmjit::x86::xmm11},
			{"xmm12", asmjit::x86::xmm12}, {"xmm13", asmjit::x86::xmm13}, {"xmm14", asmjit::x86::xmm14},
			{"xmm15", asmjit::x86::xmm15}
		};
		// clang-format on

		const auto it = reg_map.find(name);
		if (it != reg_map.end())
		{
			return it->second;
		}
		else
		{
			return std::nullopt;
		}
	}

	std::string parse_address_component(std::string_view name, std::string_view::iterator& index)
	{
		auto end = std::find_if(index,
		                        name.end(),
		                        [](char c)
		                        {
			                        return c == '+' || c == '-' || c == '*' || c == ']';
		                        });

		std::string sub_str;
		std::remove_copy_if(index,
		                    end,
		                    std::back_inserter(sub_str),
		                    [](char c)
		                    {
			                    return c == ' ';
		                    });

		index = --end;
		return sub_str;
	}

	std::optional<uint64_t> parse_number_from_string(std::string_view str)
	{
		int base;
		if (str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
		{
			str.remove_prefix(2);
			base = 16;
		}
		else
		{
			switch (str.back())
			{
			case 'b':
				str.remove_suffix(1);
				base = 2;
				break;
			case 'o':
				str.remove_suffix(1);
				base = 8;
				break;
			case 'd':
				str.remove_suffix(1);
				base = 10;
				break;
			case 'h':
				str.remove_suffix(1);
				base = 16;
				break;
			default: base = 10;
			}
		}
		uint64_t num{};
		auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), num, base);
		if (ec == std::errc())
		{
			return num;
		}
		return std::nullopt;
	}

	std::optional<asmjit::x86::Mem> get_addr_from_name(std::string_view name, const int64_t rsp_offset)
	{
		std::string base_str;
		std::string index_str;
		int32_t offset = 0;
		uint32_t shift = 0;
		for (auto it = name.begin(); it != name.end(); ++it)
		{
			switch (*it)
			{
			case '[':
			{
				std::string sub_str = parse_address_component(name, ++it);
				auto num            = parse_number_from_string(sub_str);
				if (num.has_value())
				{
					return asmjit::x86::ptr(*num);
				}
				else
				{
					base_str = sub_str;
				}
				break;
			}
			case '+':
			{
				std::string sub_str = parse_address_component(name, ++it);
				auto num            = parse_number_from_string(sub_str);
				if (num.has_value())
				{
					offset += *num;
				}
				else
				{
					index_str = sub_str;
				}
				break;
			}
			case '-':
			{
				std::string sub_str = parse_address_component(name, ++it);
				auto num            = parse_number_from_string(sub_str);
				if (num.has_value())
				{
					offset -= *num;
				}
				else
				{
					// I'm not sure this should happen.
					// I think this is not necessary in a normal situation, and may need a register to solve it.
					LOG(ERROR) << "Can't sub a register";
					return std::nullopt;
				}
				break;
			}
			case '*':
			{
				std::string sub_str = parse_address_component(name, ++it);
				auto num            = parse_number_from_string(sub_str);
				if (num.has_value())
				{
					uint64_t temp_num = *num;
					while (temp_num)
					{
						temp_num >>= 1;
						shift++;
					}
					shift--;
				}
				else
				{
					LOG(ERROR) << "Can't parse the shift";
					return std::nullopt;
				}
				break;
			}
			}
		}

		auto base = get_gp_from_name(base_str);
		if (auto base = get_gp_from_name(base_str); !base.has_value())
		{
			LOG(ERROR) << "Failed to get base reg from: " << base_str;
			return std::nullopt;
		}

		if (*base == asmjit::x86::rsp)
		{
			offset += rsp_offset;
		}

		if (auto index = get_gp_from_name(index_str); index.has_value())
		{
			return asmjit::x86::ptr(*base, *index, shift, offset);
		}
		else
		{
			return asmjit::x86::ptr(*base, offset);
		}

		LOG(ERROR) << "Failed to parse address from: " << name;
		return std::nullopt;
	}

	std::vector<uint32_t> get_useable_gp_id_from_name(std::string_view name)
	{
		std::vector<uint32_t> useable_gp_id_list = {asmjit::x86::Gp::kIdAx, asmjit::x86::Gp::kIdBx, asmjit::x86::Gp::kIdCx, asmjit::x86::Gp::kIdDx, asmjit::x86::Gp::kIdBp, asmjit::x86::Gp::kIdSi, asmjit::x86::Gp::kIdDi, asmjit::x86::Gp::kIdR8, asmjit::x86::Gp::kIdR9, asmjit::x86::Gp::kIdR10, asmjit::x86::Gp::kIdR11, asmjit::x86::Gp::kIdR12, asmjit::x86::Gp::kIdR13, asmjit::x86::Gp::kIdR14, asmjit::x86::Gp::kIdR15};
		auto delete_gp = [&](std::string str)
		{
			if (auto index = get_gp_from_name(str); index.has_value())
			{
				auto it = std::find(useable_gp_id_list.begin(), useable_gp_id_list.end(), index->id());
				if (it != useable_gp_id_list.end())
				{
					useable_gp_id_list.erase(it);
				}
			}
		};
		for (auto it = name.begin(); it != name.end(); ++it)
		{
			switch (*it)
			{
			case '[':
			{
				std::string sub_str = parse_address_component(name, ++it);
				auto num            = parse_number_from_string(sub_str);
				if (num.has_value())
				{
					return useable_gp_id_list;
				}
				else
				{
					delete_gp(sub_str);
				}
				break;
			}
			case '+':
			{
				std::string sub_str = parse_address_component(name, ++it);
				auto num            = parse_number_from_string(sub_str);
				if (!num.has_value())
				{
					delete_gp(sub_str);
				}
				break;
			}
			case '-':
			{
				std::string sub_str = parse_address_component(name, ++it);
				auto num            = parse_number_from_string(sub_str);
				if (!num.has_value())
				{
					// I'm not sure this should happen.
					// I think this is not necessary in a normal situation, and may need a register to solve it.
					LOG(ERROR) << "Can't sub a register";
					return std::vector<uint32_t>();
				}
				break;
			}
			case '*':
			{
				std::string sub_str = parse_address_component(name, ++it);
				auto num            = parse_number_from_string(sub_str);
				if (!num.has_value())
				{
					LOG(ERROR) << "Can't parse the shift";
					return std::vector<uint32_t>();
				}
				break;
			}
			}
		}
		return useable_gp_id_list;
	}
} // namespace lua::memory
