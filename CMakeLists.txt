cmake_minimum_required(VERSION 3.20)
project(eleven_quest_targets LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# v0.3.1 build fix: only repair the native build dependency.
# Do not change main.cpp or mod.json, which contain the v0.2.6 target positions.
# The moving master branch of Dobby contains Android NDK compilation regressions.
# Pin a known-working revision (also reported by other Dobby users).
include(FetchContent)
set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "" FORCE)
set(Plugin.SymbolResolver OFF CACHE BOOL "" FORCE)
FetchContent_Declare(dobby
    GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG 0932d69c320e786672361ab53825ba8f4245e9d3
    GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(dobby)

add_library(eleven_quest_targets SHARED main.cpp)
target_include_directories(eleven_quest_targets PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/external"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/scotland2/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/scotland2")
target_link_libraries(eleven_quest_targets PRIVATE dobby_static log dl)
