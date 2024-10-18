include(FetchContent)

message("cxxopts")

add_compile_definitions(CXXOPTS_WCHAR)
if(MSVC)
    add_compile_options(/wd4273)
endif()

FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/matttyson/cxxopts.git
    GIT_TAG 7f24883e44b032e4c75c5af2391072e45c6078f0
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(cxxopts)

set_property(TARGET cxxopts PROPERTY CXX_STANDARD 23)
