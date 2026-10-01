cmake_minimum_required(VERSION 3.16)
project(library_web CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SERVER "Build the Crow web server" ON)
option(BUILD_TESTS "Build the GoogleTest suite" ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
include(FetchContent)

# Business logic (no web code) so it can be unit-tested on its own.
add_library(library_core src/database.cpp src/auth.cpp src/library.cpp src/recommender.cpp)
target_include_directories(library_core PUBLIC src)
target_link_libraries(library_core PUBLIC SQLite::SQLite3)
target_compile_options(library_core PRIVATE -Wall -Wextra)

if(BUILD_TESTS)
  enable_testing()
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  add_executable(library_tests tests/test_auth.cpp tests/test_library.cpp tests/test_recommender.cpp)
  target_link_libraries(library_tests PRIVATE library_core GTest::gtest_main)
  add_test(NAME all_tests COMMAND library_tests)
endif()

if(BUILD_SERVER)
  set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(CROW_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(Crow
    URL https://github.com/CrowCpp/Crow/archive/refs/tags/v1.2.0.tar.gz)
  FetchContent_MakeAvailable(Crow)
  add_executable(library_server src/main.cpp)
  target_link_libraries(library_server PRIVATE library_core Crow::Crow Threads::Threads)
endif()
