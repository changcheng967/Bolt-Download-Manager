# Copyright (c) 2026 changcheng967. All rights reserved.

add_library(bolt_disk STATIC)

target_sources(bolt_disk PRIVATE
    async_file.cpp
    file_writer.cpp
    write_coalescer.cpp
)

target_include_directories(bolt_disk PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(bolt_disk PUBLIC
    bolt_core
)

target_compile_features(bolt_disk PUBLIC cxx_std_23)

if(MSVC)
    target_compile_definitions(bolt_disk PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
