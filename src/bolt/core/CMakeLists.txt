# Copyright (c) 2026 changcheng967. All rights reserved.

add_library(bolt_core STATIC)

target_sources(bolt_core PRIVATE
    download_engine.cpp
    http_session.cpp
    segment.cpp
    bandwidth_prober.cpp
    url.cpp
)

target_include_directories(bolt_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Find dependencies using CONFIG mode for vcpkg
find_package(CURL CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)

target_link_libraries(bolt_core PUBLIC
    CURL::libcurl
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
)

target_compile_features(bolt_core PUBLIC cxx_std_23)

if(MSVC)
    target_compile_definitions(bolt_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(bolt_core PRIVATE /wd4996)
endif()
