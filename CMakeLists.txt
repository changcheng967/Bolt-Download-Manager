cmake_minimum_required(VERSION 4.2)
project(BoltDM VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FeatureSummary)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# Windows-specific build configuration
if(WIN32)
    # Set output directories
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

    # Disable deprecated CRT warnings from curl
    if(MSVC)
        add_compile_options(/wd4996)
        if(CMAKE_BUILD_TYPE MATCHES Release)
            add_compile_options(/O2 /GL /Ob3 /Oi /Ot)
            add_link_options(/LTCG /OPT:REF /OPT:ICF)
        endif()
    endif()
endif()

# Subdirectories
add_subdirectory(src/bolt/core)
add_subdirectory(src/bolt/disk)
add_subdirectory(src/bolt/cli)

# GUI requires Qt6 - skip until installed
# add_subdirectory(src/bolt/gui)
# add_subdirectory(src/bolt/browser)
# add_subdirectory(src/bolt/media)
# add_subdirectory(tests)

# Find optional dependencies
find_package(spdlog CONFIG QUIET)
find_package(nlohmann_json CONFIG QUIET)

# Qt6 - check common installation paths
if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")
endif()
find_package(Qt6 6 COMPONENTS Core Widgets Charts Svg Network)
find_package(Catch2 3 QUIET)

# GUI application (requires Qt6)
if(Qt6_FOUND)
    add_subdirectory(src/bolt/gui)
endif()

# Unit tests (requires Catch2)
if(Catch2_FOUND)
    enable_testing()
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL)
