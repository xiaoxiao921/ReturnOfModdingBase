include(FetchContent)

message("cxxopts")

add_compile_definitions(CXXOPTS_WCHAR)
if(MSVC)
    add_compile_options(/wd4273)
endif()

FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/xiaoxiao921/cxxopts.git
    GIT_TAG fe250a7ce7ae0f7ff7a076f0577ed23a508b9b5b
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(cxxopts)

set_property(TARGET cxxopts PROPERTY CXX_STANDARD 23)
