cmake_minimum_required(VERSION 3.16)
project(num8
    VERSION 1.0.0
    DESCRIPTION "O(1) membership engine for 8-digit numbers"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(num8_static STATIC
    num8.c
)
target_include_directories(num8_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(num8_shared SHARED
    num8.c
)
target_include_directories(num8_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(num8_shared PROPERTIES
    OUTPUT_NAME num8
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

add_executable(num8_builder
    num8_builder.c
)
target_link_libraries(num8_builder PRIVATE num8_static)

add_executable(num8_selftest
    num8_selftest.c
)
target_link_libraries(num8_selftest PRIVATE num8_static)

add_executable(num8_minitest
    num8_minitest.c
)
target_link_libraries(num8_minitest PRIVATE num8_static)

add_executable(num8_bench
    num8_bench.c
)
target_link_libraries(num8_bench PRIVATE num8_static)

enable_testing()
add_test(NAME num8_selftest COMMAND num8_selftest)
add_test(NAME num8_minitest COMMAND num8_minitest)

install(TARGETS num8_static num8_shared
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES num8.h DESTINATION include)
