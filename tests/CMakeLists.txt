# Copyright (c) 2026 changcheng967. All rights reserved.

# Catch2 should already be found by parent CMakeLists
if(NOT Catch2_FOUND)
    message(FATAL_ERROR "Catch2 is required for tests but was not found")
endif()

add_executable(bolt_tests)

target_sources(bolt_tests PRIVATE
    main.cpp
    url_tests.cpp
    segment_tests.cpp
    bandwidth_tests.cpp
)

target_include_directories(bolt_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(bolt_tests PRIVATE
    bolt_core
    Catch2::Catch2WithMain
)

target_compile_features(bolt_tests PRIVATE cxx_std_23)

include(CTest)
include(Catch)
catch_discover_tests(bolt_tests)
