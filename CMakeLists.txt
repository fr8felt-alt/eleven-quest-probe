cmake_minimum_required(VERSION 3.20)
project(eleven_quest_targets LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(eleven_quest_targets SHARED main.cpp)
target_include_directories(eleven_quest_targets PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/external"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/scotland2/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/scotland2")
target_link_libraries(eleven_quest_targets PRIVATE log dl)
