# Copyright (c) 2026 changcheng967. All rights reserved.

# Qt6 should already be found by parent CMakeLists
if(NOT Qt6_FOUND)
    message(FATAL_ERROR "Qt6 is required for GUI but was not found")
endif()

# Generate Qt MOC files
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(boltdm_gui WIN32)

target_sources(boltdm_gui PRIVATE
    main.cpp
    main_window.cpp
    download_widget.cpp
    segment_widget.cpp
    speed_graph.cpp
    add_dialog.cpp
    settings_dialog.cpp
    about_dialog.cpp
    tray_icon.cpp

    # Headers for MOC processing
    ${CMAKE_SOURCE_DIR}/include/bolt/gui/main_window.hpp
    ${CMAKE_SOURCE_DIR}/include/bolt/gui/download_widget.hpp
    ${CMAKE_SOURCE_DIR}/include/bolt/gui/segment_widget.hpp
    ${CMAKE_SOURCE_DIR}/include/bolt/gui/speed_graph.hpp
    ${CMAKE_SOURCE_DIR}/include/bolt/gui/add_dialog.hpp
    ${CMAKE_SOURCE_DIR}/include/bolt/gui/settings_dialog.hpp
    ${CMAKE_SOURCE_DIR}/include/bolt/gui/about_dialog.hpp
    ${CMAKE_SOURCE_DIR}/include/bolt/gui/tray_icon.hpp
)

target_include_directories(boltdm_gui PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(boltdm_gui PRIVATE
    bolt_core
    bolt_disk
    Qt6::Core
    Qt6::Widgets
    Qt6::Charts
    Qt6::Svg
    Qt6::Network
)

target_compile_features(boltdm_gui PRIVATE cxx_std_23)

if(MSVC)
    target_compile_definitions(boltdm_gui PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# Set application metadata
set_target_properties(boltdm_gui PROPERTIES
    OUTPUT_NAME "BoltDM"
)
