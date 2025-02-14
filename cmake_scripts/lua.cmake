include(FetchContent)

message("lua")

if (NOT DEFINED LUA_USE_LUAJIT)
	set(LUA_USE_LUAJIT true)
endif()

if (LUA_USE_LUAJIT)
    add_compile_definitions(LUA_USE_LUAJIT)

	# Set the LuaJIT repository and tag to compiled by luajit-cmake
	set(LUAJIT_GIT_REPOSITORY "https://github.com/openresty/luajit2")
	set(LUAJIT_GIT_TAG "v2.1-20250117")

	# Include the luajit-cmake project with the custom repository and tag
	# luajit-cmake will download and compile the above fork of LuaJIT
	FetchContent_Declare(
		luajitcmake
		GIT_REPOSITORY https://github.com/vworlds/luajit-cmake.git
		GIT_TAG master  # Replace with the desired commit/tag
	)

	FetchContent_MakeAvailable(luajitcmake)

else()

	project(lua LANGUAGES C VERSION 5.2.1)

	if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24")
		cmake_policy(SET CMP0135 NEW)
	endif()

	option(LUA_SUPPORT_DL "Support dynamic loading of compiled modules" ON)
	option(LUA_BUILD_AS_CXX "Build lua as C++" OFF)
	option(LUA_BUILD_BINARY "Build lua binary" OFF)
	option(LUA_BUILD_COMPILER "Build luac compiler" OFF)
	option(LUA_BUILD_DLL "Build lua as DLL" OFF)

	if (NOT DEFINED LUA_GIT_HASH)
		set(LUA_GIT_HASH c8e96d6e91dc2e3d5b175cc4cd811398ab35c82d) # Default to 5.2.2
	endif()

	if (NOT DEFINED LUA_CUSTOM_REPO)
		set(LUA_CUSTOM_REPO https://github.com/lua/lua.git)
	endif()

	if (LUA_BUILD_DLL)
		add_compile_definitions(LUA_BUILD_AS_DLL)
	endif()

	message("lua used git hash: ${LUA_GIT_HASH}")

	message("lua patch: ${LUA_PATCH_PATH}")
	if (DEFINED LUA_PATCH_PATH)
		set(LUA_PATCH git apply --3way ${LUA_PATCH_PATH})
	endif()

	FetchContent_Declare(lua_static
		GIT_REPOSITORY ${LUA_CUSTOM_REPO
		GIT_TAG        ${LUA_GIT_HASH}
		PATCH_COMMAND ${LUA_PATCH}
        UPDATE_DISCONNECTED 1
	)
	FetchContent_GetProperties(lua_static)
	FetchContent_Populate(lua_static)

	file(GLOB LUA_LIB_SRCS
		"${lua_static_SOURCE_DIR}/*.c"
	)

	if(LUA_BUILD_AS_CXX)
		set_source_files_properties(${LUA_LIB_SRCS} ${lua_static_SOURCE_DIR}/lua.c ${lua_static_SOURCE_DIR}/luac.c PROPERTIES LANGUAGE CXX )
	endif()

	set(CMAKE_SHARED_LIBRARY_PREFIX "")
	add_library(lua_static STATIC ${LUA_LIB_SRCS})
	source_group(TREE ${lua_static_SOURCE_DIR} PREFIX "lua" FILES ${LUA_LIB_SRCS})
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

endif()

message("sol2")
FetchContent_Declare(
	sol2
	GIT_REPOSITORY "https://github.com/ThePhD/sol2.git"
	GIT_SHALLOW ON
    GIT_SUBMODULES ""
	GIT_TAG "v3.3.0"
)
FetchContent_MakeAvailable(sol2)
