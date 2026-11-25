cmake_minimum_required(VERSION 3.20)
project(eleven_quest_targets LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Fetch a static ARM64 inline hook engine during GitHub Actions.
# The existing workflow's Scotland2 header download remains unchanged.
include(FetchContent)
set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(Plugin.SymbolResolver OFF CACHE BOOL "" FORCE)
FetchContent_Declare(dobby GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG master GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(dobby)
add_library(eleven_quest_targets SHARED main.cpp)
target_include_directories(eleven_quest_targets PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/external"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/scotland2/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/scotland2")
target_link_libraries(eleven_quest_targets PRIVATE dobby_static log dl)
