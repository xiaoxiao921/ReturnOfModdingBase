include(FetchContent)

message("toml++")
FetchContent_Declare(
	toml++
	GIT_REPOSITORY "https://github.com/marzer/tomlplusplus.git"
	GIT_SHALLOW ON
    GIT_SUBMODULES ""
	GIT_TAG "v3.4.0"
)
FetchContent_MakeAvailable(toml++)