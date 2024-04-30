include(FetchContent)

message("lua")

project(lua LANGUAGES C VERSION 5.2.1)

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
endif()

option(LUA_SUPPORT_DL "Support dynamic loading of compiled modules" ON)
option(LUA_BUILD_AS_CXX "Build lua as C++" OFF)
option(LUA_BUILD_BINARY "Build lua binary" OFF)
option(LUA_BUILD_COMPILER "Build luac compiler" OFF)
option(LUA_BUILD_DLL "Build lua as DLL" OFF)
set(LUA_GIT_HASH c8e96d6e91dc2e3d5b175cc4cd811398ab35c82d CACHE STRING "Build git hash. Default to 5.2.2")

if (LUA_BUILD_DLL)
    add_compile_definitions(LUA_BUILD_AS_DLL)
endif()

message("lua used git hash: ${LUA_GIT_HASH}")

FetchContent_Declare(lua_static
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        ${LUA_GIT_HASH}
)
FetchContent_GetProperties(lua_static)
FetchContent_Populate(lua_static)

set(LUA_LIB_SRCS
    ${lua_static_SOURCE_DIR}/lapi.c
    ${lua_static_SOURCE_DIR}/lauxlib.c
    ${lua_static_SOURCE_DIR}/lbaselib.c
    ${lua_static_SOURCE_DIR}/lbitlib.c
    ${lua_static_SOURCE_DIR}/lcode.c
    ${lua_static_SOURCE_DIR}/lcorolib.c
    ${lua_static_SOURCE_DIR}/lctype.c
    ${lua_static_SOURCE_DIR}/ldblib.c
    ${lua_static_SOURCE_DIR}/ldebug.c
    ${lua_static_SOURCE_DIR}/ldo.c
    ${lua_static_SOURCE_DIR}/ldump.c
    ${lua_static_SOURCE_DIR}/lfunc.c
    ${lua_static_SOURCE_DIR}/lgc.c
    ${lua_static_SOURCE_DIR}/linit.c
    ${lua_static_SOURCE_DIR}/liolib.c
    ${lua_static_SOURCE_DIR}/llex.c
    ${lua_static_SOURCE_DIR}/lmathlib.c
    ${lua_static_SOURCE_DIR}/lmem.c
    ${lua_static_SOURCE_DIR}/loadlib.c
    ${lua_static_SOURCE_DIR}/lobject.c
    ${lua_static_SOURCE_DIR}/lopcodes.c
    ${lua_static_SOURCE_DIR}/loslib.c
    ${lua_static_SOURCE_DIR}/lparser.c
    ${lua_static_SOURCE_DIR}/lstate.c
    ${lua_static_SOURCE_DIR}/lstring.c
    ${lua_static_SOURCE_DIR}/lstrlib.c
    ${lua_static_SOURCE_DIR}/ltable.c
    ${lua_static_SOURCE_DIR}/ltablib.c
    ${lua_static_SOURCE_DIR}/ltm.c
    ${lua_static_SOURCE_DIR}/lundump.c
    ${lua_static_SOURCE_DIR}/lvm.c
    ${lua_static_SOURCE_DIR}/lzio.c
)

if(LUA_BUILD_AS_CXX)
	set_source_files_properties(${LUA_LIB_SRCS} ${lua_static_SOURCE_DIR}/lua.c ${lua_static_SOURCE_DIR}/luac.c PROPERTIES LANGUAGE CXX )
endif()

set(CMAKE_SHARED_LIBRARY_PREFIX "")
add_library(lua_static STATIC ${LUA_LIB_SRCS})
set_target_properties(lua_static PROPERTIES OUTPUT_NAME "lua52")
target_include_directories(lua_static PUBLIC ${lua_static_SOURCE_DIR})
if(UNIX)
    if (UNIX AND NOT(EMSCRIPTEN))
        find_library(LIBM m)
        if(NOT LIBM)
            message(FATAL_ERROR "libm not found and is required by lua")
        endif()
        target_link_libraries(lua_static PUBLIC ${LIBM})
    endif()
endif()

if (WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif ()

install(TARGETS lua_static)

set(LUA_INCLUDE_DIR "${lua_static_SOURCE_DIR}")
set(LUA_LIBRARIES lua_static)

message("sol2")
FetchContent_Declare(
	sol2
	GIT_REPOSITORY "https://github.com/ThePhD/sol2.git"
	GIT_SHALLOW ON
    GIT_SUBMODULES ""
	GIT_TAG "v3.3.0"
)
FetchContent_MakeAvailable(sol2)
