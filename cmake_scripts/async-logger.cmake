include(FetchContent)

message("AsyncLogger")
FetchContent_Declare(
    AsyncLogger
    GIT_REPOSITORY https://github.com/xiaoxiao921/AsyncLogger.git
    GIT_TAG 364a1a92e74718185e56f7921c555f027312e4ec
)
FetchContent_MakeAvailable(AsyncLogger)

set_property(TARGET AsyncLogger PROPERTY CXX_STANDARD 23)
