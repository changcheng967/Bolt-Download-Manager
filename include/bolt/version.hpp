// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <cstdint>
#include <format>
#include <string_view>

namespace bolt {

constexpr struct Version {
    std::uint32_t major = 0;
    std::uint32_t minor = 1;
    std::uint32_t patch = 0;

    constexpr auto operator<=>(const Version&) const = default;

    [[nodiscard]] constexpr std::uint64_t to_number() const noexcept {
        return (static_cast<std::uint64_t>(major) << 32)
             | (static_cast<std::uint64_t>(minor) << 16)
             | static_cast<std::uint64_t>(patch);
    }

    [[nodiscard]] std::string to_string() const {
        return std::format("{}.{}.{}", major, minor, patch);
    }
} version;

constexpr std::string_view BUILD_DATE = __DATE__;
constexpr std::string_view BUILD_TIME = __TIME__;

} // namespace bolt
