include(FetchContent)

FetchContent_Declare(
	polyhook
	GIT_REPOSITORY https://github.com/stevemk14ebr/PolyHook_2_0.git
	GIT_TAG 8cd6cb4ef0f2a599f35bdd46ac5833843aea6523
)
FetchContent_MakeAvailable(polyhook)