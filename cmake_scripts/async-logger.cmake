include(FetchContent)

message("AsyncLogger")
FetchContent_Declare(
    AsyncLogger
    GIT_REPOSITORY https://github.com/xiaoxiao921/AsyncLogger.git
    GIT_TAG e32d649a412ca31dbf56e510371c2e16e7c43b96
)
FetchContent_MakeAvailable(AsyncLogger)

set_property(TARGET AsyncLogger PROPERTY CXX_STANDARD 23)
