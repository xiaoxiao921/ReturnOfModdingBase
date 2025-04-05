include(FetchContent)

message("hashmap")
FetchContent_Declare(
	hashmap
	GIT_REPOSITORY "https://github.com/martinus/unordered_dense.git"
	GIT_TAG 73f3cbb237e84d483afafc743f1f14ec53e12314
)
FetchContent_MakeAvailable(hashmap)