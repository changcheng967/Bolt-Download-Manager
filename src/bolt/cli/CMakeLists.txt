# Copyright (c) 2026 changcheng967. All rights reserved.

add_executable(boltdm-cli)

target_sources(boltdm-cli PRIVATE
    main.cpp
    commands.cpp
    progress_bar.cpp
)

target_include_directories(boltdm-cli PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(boltdm-cli PRIVATE
    bolt_core
    bolt_disk
)

target_compile_features(boltdm-cli PRIVATE cxx_std_23)

if(MSVC)
    target_compile_definitions(boltdm-cli PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
