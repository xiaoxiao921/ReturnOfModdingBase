#include "gui.hpp"

#include "gui_element.hpp"
#include "raw_imgui_callback.hpp"

namespace lua::gui
{
	static void add_menu_bar_callback(sol::this_environment& env, std::unique_ptr<lua::gui::gui_element> element)
	{
		big::lua_module* module = big::lua_module::this_from(env);
		if (module)
		{
			module->m_data.m_menu_bar_callbacks.push_back(std::move(element));
		}
	}

	static void add_always_draw_independent_element(sol::this_environment& env, std::unique_ptr<lua::gui::gui_element> element)
	{
		big::lua_module* module = big::lua_module::this_from(env);
		if (module)
		{
			module->m_data.m_always_draw_independent_gui.push_back(std::move(element));
		}
	}

	static void add_independent_element(sol::this_environment& env, std::unique_ptr<lua::gui::gui_element> element)
	{
		big::lua_module* module = big::lua_module::this_from(env);
		if (module)
		{
			module->m_data.m_independent_gui.push_back(std::move(element));
		}
	}

	// Lua API: Function
	// Table: gui
	// Name: add_to_menu_bar
	// Param: imgui_rendering: function: Function that will be called under your dedicated space in the imgui main menu bar.
	// Registers a function that will be called under your dedicated space in the imgui main menu bar.
	// **Example Usage:**
	// ```lua
	// gui.add_to_menu_bar(function()
	//   if ImGui.BeginMenu("Ayo") then
	//       if ImGui.Button("Label") then
	//         log.info("hi")
	//       end
	//       ImGui.EndMenu()
	//   end
	// end)
	// ```
	static lua::gui::raw_imgui_callback* add_to_menu_bar(sol::protected_function imgui_rendering, sol::this_environment state)
	{
		auto element = std::make_unique<lua::gui::raw_imgui_callback>(imgui_rendering);
		auto el_ptr  = element.get();
		add_menu_bar_callback(state, std::move(element));
		return el_ptr;
	}

	// Lua API: Function
	// Table: gui
	// Name: add_always_draw_imgui
	// Param: imgui_rendering: function: Function that will be called every rendering frame, regardless of the gui is in its open state. You can call ImGui functions in it, please check the ImGui.md documentation file for more info.
	// Registers a function that will be called every rendering frame, regardless of the gui is in its open state. You can call ImGui functions in it, please check the ImGui.md documentation file for more info.
	// **Example Usage:**
	// ```lua
	// gui.add_always_draw_imgui(function()
	//   if ImGui.Begin("My Custom Window") then
	//       if ImGui.Button("Label") then
	//         log.info("hi")
	//       end
	//
	//   end
	//   ImGui.End()
	// end)
	// ```
	static lua::gui::raw_imgui_callback* add_always_draw_imgui(sol::protected_function imgui_rendering, sol::this_environment state)
	{
		auto element = std::make_unique<lua::gui::raw_imgui_callback>(imgui_rendering);
		auto el_ptr  = element.get();
		add_always_draw_independent_element(state, std::move(element));
		return el_ptr;
	}

	// Lua API: Function
	// Table: gui
	// Name: add_imgui
	// Param: imgui_rendering: function: Function that will be called every rendering frame, only if the gui is in its open state. You can call ImGui functions in it, please check the ImGui.md documentation file for more info.
	// Registers a function that will be called every rendering frame, only if the gui is in its open state. You can call ImGui functions in it, please check the ImGui.md documentation file for more info.
	// **Example Usage:**
	// ```lua
	// gui.add_imgui(function()
	//   if ImGui.Begin("My Custom Window") then
	//       if ImGui.Button("Label") then
	//         log.info("hi")
	//       end
	//
	//   end
	//   ImGui.End()
	// end)
	// ```
	static lua::gui::raw_imgui_callback* add_imgui(sol::protected_function imgui_rendering, sol::this_environment state)
	{
		auto element = std::make_unique<lua::gui::raw_imgui_callback>(imgui_rendering);
		auto el_ptr  = element.get();
		add_independent_element(state, std::move(element));
		return el_ptr;
	}

	std::function<bool()> g_on_is_open = nullptr;

	// Lua API: Function
	// Table: gui
	// Name: is_open
	// Returns: boolean: Returns true if the GUI is open.
	static bool is_open()
	{
		if (g_on_is_open)
		{
			return g_on_is_open();
		}

		return false;
	}

	std::function<void()> g_on_toggle = nullptr;

	// Lua API: Function
	// Table: gui
	// Name: toggle
	// Opens or closes the GUI.
	static void toggle()
	{
		if (g_on_toggle)
		{
			g_on_toggle();
		}
	}

	void bind(sol::table& state)
	{
		auto ns                     = state.create_named("gui");
		ns["add_imgui"]             = add_imgui;
		ns["add_always_draw_imgui"] = add_always_draw_imgui;
		ns["add_to_menu_bar"]       = add_to_menu_bar;

		ns["is_open"] = is_open;
		ns["toggle"]  = toggle;
	}
} // namespace lua::gui
