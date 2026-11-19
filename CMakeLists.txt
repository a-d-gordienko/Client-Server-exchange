cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)  # find_package(Boost) учитывает BOOST_ROOT
endif()

project(ClientServerExchange LANGUAGES CXX)

enable_testing()  # иначе ctest из корня build не видит тесты подкаталога

option(CSE_USE_SYSTEM_GTEST "Use find_package(GTest) instead of FetchContent" OFF)

set(BOOST_ROOT "D:/libs/boost_1_92_0" CACHE PATH "Boost 1.92 headers location")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CompilerWarnings.cmake)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_WARNINGS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(spdlog-1.x)

add_subdirectory(Core)
add_subdirectory(Tests)

add_executable(cse_server Server/main.cpp)
target_link_libraries(cse_server PRIVATE cse_core cse_warnings)

add_executable(cse_client Client/main.cpp)
target_link_libraries(cse_client PRIVATE cse_core cse_warnings)
