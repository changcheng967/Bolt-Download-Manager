// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <string>
#include <iostream>
#include <sstream>

// C++23 std::format and std::println compatibility for MSVC
#ifndef BOLT_COMPAT_HPP
#define BOLT_COMPAT_HPP

namespace bolt::compat {

// Simple format function using ostringstream
template<typename... Args>
inline std::string format(Args&&... args) {
    std::ostringstream ss;
    (ss << ... << args);
    return ss.str();
}

// Simple println using std::cout
template<typename... Args>
inline void println(Args&&... args) {
    (std::cout << ... << args) << std::endl;
}

// Simple println to specific stream
template<typename... Args>
inline void println(std::ostream& os, Args&&... args) {
    (os << ... << args) << std::endl;
}

} // namespace bolt::compat

#endif // BOLT_COMPAT_HPP
