include(FetchContent)

message("AsyncLogger")
FetchContent_Declare(
    AsyncLogger
    GIT_REPOSITORY https://github.com/xiaoxiao921/AsyncLogger.git
    GIT_TAG 479d2385c2420a771499cc816f75e584632dc946
)
FetchContent_MakeAvailable(AsyncLogger)

set_property(TARGET AsyncLogger PROPERTY CXX_STANDARD 23)
