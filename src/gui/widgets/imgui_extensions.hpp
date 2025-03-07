#pragma once

#include "toml_v2/config_file.hpp"

#include <type_traits>

namespace ImGui
{
	inline bool SliderDouble(const char* label, double* v, double v_min, double v_max)
	{
		return SliderScalar(label, ImGuiDataType_Double, v, &v_min, &v_max);
	}

	inline bool DragDouble(const char* label, double* v, float v_speed, double v_min, double v_max)
	{
		return DragScalar(label, ImGuiDataType_Double, v, v_speed, &v_min, &v_max);
	}

	template<typename T, typename Func, typename... Args>
	inline void ConfigBind(Func imgui_func, const char* label, toml_v2::config_file::config_entry<T>* entry, Args&&... args)
	{
		T value = entry->get_value();
		if (imgui_func(label, &value, std::forward<Args>(args)...))
		{
			entry->set_value(value);
		}
	}
} // namespace ImGui
