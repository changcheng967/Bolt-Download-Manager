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

# Try to find dependencies, allow building without them for testing
find_package(CURL QUIET)
if(CURL_FOUND)
    target_link_libraries(bolt_core PUBLIC ${CURL_LIBRARIES})
    target_include_directories(bolt_core PRIVATE ${CURL_INCLUDE_DIRS})
else()
    message(WARNING "libcurl not found, linking may fail")
endif()

find_package(Boost COMPONENTS system QUIET)
if(Boost_FOUND)
    target_link_libraries(bolt_core PUBLIC Boost::system)
    target_include_directories(bolt_core PRIVATE ${Boost_INCLUDE_DIRS})
else()
    message(WARNING "Boost.Asio not found, linking may fail")
endif()

# Note: nlohmann_json and spdlog are header-only, no linking needed
# OpenSSL and curl will be handled by system libs if vcpkg is not set

target_compile_features(bolt_core PUBLIC cxx_std_23)

if(MSVC)
    target_compile_definitions(bolt_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(bolt_core PRIVATE /wd4996)  # Disable secure CRT warnings for curl
endif()
