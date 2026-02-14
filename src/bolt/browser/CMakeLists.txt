# Copyright (c) 2026 changcheng967. All rights reserved.

# nlohmann_json should already be found by parent CMakeLists
if(NOT nlohmann_json_FOUND)
    message(FATAL_ERROR "nlohmann_json is required for browser native host but was not found")
endif()

add_executable(boltdm_host)

target_sources(boltdm_host PRIVATE
    native_host.cpp
)

target_include_directories(boltdm_host PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(boltdm_host PRIVATE
    bolt_core
    bolt_disk
    nlohmann_json::nlohmann_json
)

target_compile_features(boltdm_host PRIVATE cxx_std_23)

if(MSVC)
    target_compile_definitions(boltdm_host PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
