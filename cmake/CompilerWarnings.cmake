add_library(cse_warnings INTERFACE)

if(MSVC)
    target_compile_options(cse_warnings INTERFACE /W4 /permissive- /utf-8)
else()
    target_compile_options(cse_warnings INTERFACE -Wall -Wextra)
endif()
