enable_testing()

if(CSE_USE_SYSTEM_GTEST)
    find_package(GTest REQUIRED)
else()
    include(FetchContent)
    # plan-URL ".../releases/download/v1.15.2/googletest-1.15.2.zip" отдаёт 404,
    # у gtest в релизах нет zip-ассета — берём архив тега.
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(cse_tests
    smoke_test.cpp
    logger_test.cpp
    numbers_store_test.cpp
    mean_of_squares_test.cpp
    locked_repository_test.cpp
    protocol_test.cpp
    random_source_test.cpp
    stop_source_test.cpp
    options_test.cpp
    dump_writer_test.cpp
    session_test.cpp
    integration_server_test.cpp
    integration_client_test.cpp
)

target_include_directories(cse_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cse_tests PRIVATE cse_core GTest::gtest_main cse_warnings)

include(GoogleTest)
gtest_discover_tests(cse_tests)
