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
    # Use MSVC by default on Windows for better C++20 support
    # Ninja + MSVC is a better combo than clang-cl
    include(Configuration/CMakeClAndViewSelection)
    include(Configuration/CMakeClSettings)

    # Set output directories
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

    # MSVC compilation flags
    add_compile_options(/wd4996)  # Disable deprecated CRT warnings from curl
    if(CMAKE_BUILD_TYPE MATCHES Release)
        add_compile_options(/O2 /GL /Ob3 /Oi /Ot)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
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

feature_summary(WHAT ALL)
