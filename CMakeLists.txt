cmake_minimum_required(VERSION 3.16)
project(candidate_scoring)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

add_executable(candidate_scoring main.cpp)

target_include_directories(candidate_scoring PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(candidate_scoring PRIVATE SQLite::SQLite3 Threads::Threads)
