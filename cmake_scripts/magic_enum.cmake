include(FetchContent)

message("magic_enum")
FetchContent_Declare(
	magic_enum
	GIT_REPOSITORY "https://github.com/Neargye/magic_enum.git"
	GIT_SHALLOW ON
    GIT_SUBMODULES ""
	GIT_TAG "v0.9.5"
)
FetchContent_MakeAvailable(magic_enum)