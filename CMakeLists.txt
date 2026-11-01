cmake_minimum_required(VERSION 3.20)
project(venice_exec CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(luau GIT_REPOSITORY https://github.com/luau-lang/luau.git
                          GIT_TAG master)   # pin a commit once it builds
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(luau)

# Loader
add_executable(VeniceExec WIN32 src/injector/main.cpp)
target_link_libraries(VeniceExec PRIVATE user32 comctl32)

# Injected core
add_library(VeniceCore SHARED src/core/dllmain.cpp)
target_include_directories(VeniceCore PRIVATE ${luau_SOURCE_DIR}/VM/include)
target_link_libraries(VeniceCore PRIVATE Luau.VM)