cmake_minimum_required(VERSION 3.16)
project(num8
    VERSION 1.0.0
    DESCRIPTION "O(1) membership engine for 8-digit numbers"
    LANGUAGES C
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(NUM8_BUILD_SHARED "Build the shared library" ON)
option(NUM8_BUILD_TESTS "Build tests" ON)
option(NUM8_BUILD_TOOLS "Build command-line tools" ON)
option(NUM8_BUILD_BENCHMARKS "Build benchmarks" ON)

set(NUM8_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/num8")

add_library(num8 STATIC
    num8.c
)
target_include_directories(num8
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(num8 PROPERTIES
    OUTPUT_NAME num8_static
)

if(NUM8_BUILD_SHARED)
    add_library(num8_shared SHARED
        num8.c
    )
    target_include_directories(num8_shared
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    set_target_properties(num8_shared PROPERTIES
        OUTPUT_NAME num8
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

if(NUM8_BUILD_TOOLS)
    add_executable(num8_builder
        num8_builder.c
    )
    target_link_libraries(num8_builder PRIVATE num8)
endif()

if(NUM8_BUILD_TESTS)
    add_executable(num8_selftest
        num8_selftest.c
    )
    target_link_libraries(num8_selftest PRIVATE num8)

    add_executable(num8_minitest
        num8_minitest.c
    )
    target_link_libraries(num8_minitest PRIVATE num8)
endif()

if(NUM8_BUILD_BENCHMARKS)
    add_executable(num8_bench
        num8_bench.c
    )
    target_link_libraries(num8_bench PRIVATE num8)
endif()

if(NUM8_BUILD_TESTS)
    enable_testing()
    add_test(NAME num8_selftest COMMAND num8_selftest)
    add_test(NAME num8_minitest COMMAND num8_minitest)
endif()

if(NUM8_BUILD_TESTS)
    set(NUM8_TEST_INSTALL_CONFIG "Release" CACHE STRING "Configuration used for install-based tests")
    set(NUM8_EXTERNAL_CONSUMER_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/num8-external-prefix")
    set(NUM8_EXTERNAL_CONSUMER_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/num8-external-consumer-src")
    set(NUM8_EXTERNAL_CONSUMER_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/num8-external-consumer-build")
    set(NUM8_EXTERNAL_CONSUMER_GENERATOR "${CMAKE_GENERATOR}")
    set(NUM8_EXTERNAL_CONSUMER_PLATFORM "${CMAKE_GENERATOR_PLATFORM}")
    set(NUM8_EXTERNAL_CONSUMER_TOOLSET "${CMAKE_GENERATOR_TOOLSET}")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/num8_external_consumer.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/num8_external_consumer.cmake
        @ONLY
    )
    add_test(
        NAME num8_external_consumer
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/num8_external_consumer.cmake
    )
endif()

set(NUM8_PACKAGE_SOURCES num8)
if(NUM8_BUILD_SHARED)
    list(APPEND NUM8_PACKAGE_SOURCES num8_shared)
endif()

install(TARGETS ${NUM8_PACKAGE_SOURCES}
    EXPORT num8Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES num8.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/num8ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/num8Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/num8Config.cmake
    INSTALL_DESTINATION ${NUM8_INSTALL_CMAKEDIR}
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/num8Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/num8ConfigVersion.cmake
    DESTINATION ${NUM8_INSTALL_CMAKEDIR}
)

install(EXPORT num8Targets
    NAMESPACE num8::
    DESTINATION ${NUM8_INSTALL_CMAKEDIR}
)
