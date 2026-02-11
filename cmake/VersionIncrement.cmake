// Copyright (c) 2026 changcheng967. All rights reserved.

# Auto-increment patch version on each build
function(auto_increment_version version_file)
    # Read current version.hpp content
    file(READ "${version_file}" content)

    # Find and increment patch number
    string(REGEX REPLACE "patch = ([0-9]+)" "patch = [expr \\1 + 1]" new_content "${content}")

    # Only write if changed (avoid rebuild loops)
    if(NOT "${content}" STREQUAL "${new_content}")
        file(WRITE "${version_file}" "${new_content}")
        message(STATUS "Incremented version in ${version_file}")
    endif()
endfunction()
