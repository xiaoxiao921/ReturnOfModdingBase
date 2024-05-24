include(FetchContent)

message("AsyncLogger")
FetchContent_Declare(
    AsyncLogger
    GIT_REPOSITORY https://github.com/xiaoxiao921/AsyncLogger.git
    GIT_TAG 6a52a826cd5f615beed167aa097baa0533c122b6
)
FetchContent_MakeAvailable(AsyncLogger)

set_property(TARGET AsyncLogger PROPERTY CXX_STANDARD 23)
