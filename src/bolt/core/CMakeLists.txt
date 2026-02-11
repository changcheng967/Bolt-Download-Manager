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

target_link_libraries(bolt_core PUBLIC
    CURL::libcurl
    Boost::asio
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_compile_features(bolt_core PUBLIC cxx_std_23)

if(MSVC)
    target_compile_definitions(bolt_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
