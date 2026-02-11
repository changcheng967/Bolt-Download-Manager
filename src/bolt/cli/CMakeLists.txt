# Copyright (c) 2026 changcheng967. All rights reserved.

add_executable(boltdm)

target_sources(boltdm PRIVATE
    main.cpp
    commands.cpp
    progress_bar.cpp
)

target_include_directories(boltdm PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(boltdm PRIVATE
    bolt_core
    bolt_disk
    spdlog::spdlog
)

target_compile_features(boltdm PRIVATE cxx_std_23)

if(MSVC)
    target_compile_definitions(boltdm PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Set subsystem to console
    set_target_properties(boltdm PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()
