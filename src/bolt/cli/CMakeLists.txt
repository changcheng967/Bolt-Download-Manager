# Copyright (c) 2026 changcheng967. All rights reserved.

add_executable(boltdm)

target_sources(boltdm PRIVATE
    main.cpp
    commands.cpp
    progress_bar.cpp
)

target_include_directories(boltdm PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(boltdm PRIVATE
    bolt_core
    bolt_disk
)

target_compile_features(boltdm PRIVATE cxx_std_23)

if(MSVC)
    target_compile_definitions(boltdm PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Get list of DLLs to copy from vcpkg bin
file(GLOB "${VCPKG_INSTALLED_DIR}/x64-windows/bin/*.dll" DLL_FILES)

# Create custom command to copy each DLL
foreach(DLL_FILE ${DLL_FILES})
    get_filename_component(${DLL_FILE} NAME EXT)
    get_filename_component(${DLL_FILE} NAME WE)

    add_custom_command(copy_${WE} ALL
        COMMAND ${CMAKE_COMMAND} -E copy "${DLL_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/" || exit 0
        COMMENT "Copy ${WE} to output directory"
    endforeach()

feature_summary(WHAT ALL)
