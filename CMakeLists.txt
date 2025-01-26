cmake_minimum_required(VERSION 3.20)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(ReturnOfModdingBase CXX C ASM_MASM)

set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")

# Fetch modules
message("\nFetching modules")
include(cmake_scripts/async-logger.cmake)
include(cmake_scripts/cpp-semver.cmake)
include(cmake_scripts/imgui.cmake)
include(cmake_scripts/json.cmake)
include(cmake_scripts/lua.cmake)
include(cmake_scripts/magic_enum.cmake)
include(cmake_scripts/polyhook.cmake)
include(cmake_scripts/pugixml.cmake)
include(cmake_scripts/toml.cmake)

message(STATUS "ReturnOfModdingBase")
file(GLOB_RECURSE SRC_MAIN
    "${SRC_DIR}/**.hpp"
    "${SRC_DIR}/**.h"
    "${SRC_DIR}/**.cpp"
    "${SRC_DIR}/**.cc"
    "${SRC_DIR}/**.cxx"
    "${SRC_DIR}/**.asm"
)

add_compile_options(/bigobj)
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_executable(ReturnOfModdingBase WIN32 "${SRC_MAIN}")
#add_library(ReturnOfModdingBase STATIC "${SRC_MAIN}")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(TARGET ReturnOfModdingBase PROPERTY CXX_STANDARD 23)

source_group(TREE ${SRC_DIR} PREFIX "src" FILES ${SRC_MAIN})

target_include_directories(ReturnOfModdingBase PUBLIC
    "${SRC_DIR}"
    "${json_SOURCE_DIR}/single_include"
    "${imgui_SOURCE_DIR}"
    "${polyhook_SOURCE_DIR}"
    "${cpp-semver_SOURCE_DIR}/include"
    "${toml++_SOURCE_DIR}"
    "${sol2_SOURCE_DIR}/include"
    "${magic_enum_SOURCE_DIR}/include"
)

if (LUA_USE_LUAJIT)
    target_link_libraries(ReturnOfModdingBase PUBLIC libluajit)
else()

    target_include_directories(ReturnOfModdingBase PUBLIC
        ${LUA_INCLUDE_DIR}
    )

    target_link_libraries(ReturnOfModdingBase PUBLIC lua_static)
endif()
target_link_libraries(ReturnOfModdingBase PUBLIC pugixml AsyncLogger dbghelp imgui sol2 tomlplusplus::tomlplusplus magic_enum PolyHook_2)

# Warnings as errors
set_property(TARGET ReturnOfModdingBase PROPERTY COMPILE_WARNING_AS_ERROR ON)

add_compile_definitions(ReturnOfModdingBase
    "_CRT_SECURE_NO_WARNINGS"
    "NOMINMAX"
    "WIN32_LEAN_AND_MEAN"
)