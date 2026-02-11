# Copyright (c) 2026 changcheng967. All rights reserved.

add_library(bolt_media STATIC)

target_sources(bolt_media PRIVATE
    hls_parser.cpp
    dash_parser.cpp
    media_downloader.cpp
)

target_include_directories(bolt_media PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(bolt_media PUBLIC
    bolt_core
    nlohmann_json::nlohmann_json
)

target_compile_features(bolt_media PUBLIC cxx_std_23)

if(MSVC)
    target_compile_definitions(bolt_media PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
