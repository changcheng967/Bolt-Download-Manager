cmake_minimum_required(VERSION 4.2)
project(BoltDM VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FeatureSummary)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# Compiler flags for clang-cl
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(/std:c++23 /W4 /permissive- /wd4996)
    if(CMAKE_BUILD_TYPE MATCHES Release)
        add_compile_options(/O2 /GL /Ob3 /Oi /Ot)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
    endif()
endif()

# Subdirectories
# Note: vcpkg is required for full build. Install dependencies first:
# vcpkg install curl boost-asio qt6 qt6-charts qt6-svg nlohmann-json spdlog openssl catch2
add_subdirectory(src/bolt/core)
add_subdirectory(src/bolt/disk)
# GUI requires Qt6 - skip until installed
# add_subdirectory(src/bolt/cli)
# add_subdirectory(src/bolt/gui)
# add_subdirectory(src/bolt/browser)
# add_subdirectory(src/bolt/media)
# add_subdirectory(tests)

feature_summary(WHAT ALL)

# Add placeholder for Windows-specific build (using clang-cl directly for now)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT DEFINED BOLT_SIMPLE_BUILD)
    set(BOLT_SIMPLE_BUILD ON)
    add_compile_options(/std:c++23 /W4 /permissive-)
    # Simple build directly with clang-cl
    set(CMAKE_EXE_LINKER \"C:\\Program Files\\LLVM\\bin\\lld-link.exe\")
endif()
