include(FetchContent)

set(JSON_MultipleHeaders OFF)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        9f40a7b454befb6e836b493b41df9e64c7a6fd63
    GIT_PROGRESS TRUE
)
message("json")
FetchContent_MakeAvailable(json)
