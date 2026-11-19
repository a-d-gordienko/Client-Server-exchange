add_library(cse_core STATIC
    src/logger.cpp
    src/dump_writer.cpp
)

target_include_directories(cse_core PUBLIC include)
target_link_libraries(cse_core PUBLIC cse_warnings spdlog::spdlog)

find_package(Boost CONFIG QUIET)
if(Boost_FOUND)
    target_link_libraries(cse_core INTERFACE Boost::headers)
else()
    target_include_directories(cse_core SYSTEM INTERFACE ${BOOST_ROOT})
endif()
