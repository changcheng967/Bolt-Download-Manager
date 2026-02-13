// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/core/url.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <expected>
#include <system_error>

namespace bolt::core {

// Segment metadata for persistence
struct SegmentMeta {
    std::uint32_t id{0};
    std::uint64_t offset{0};
    std::uint64_t size{0};
    std::uint64_t file_offset{0};
    std::uint64_t downloaded{0};
};

// Download metadata for .boltmeta files
struct DownloadMeta {
    std::string url;
    std::string output_path;
    std::uint64_t file_size{0};
    std::uint64_t total_downloaded{0};
    std::vector<SegmentMeta> segments;

    // Get the .boltmeta path for a given output file
    [[nodiscard]] static std::string meta_path(std::string_view output_path);

    // Save metadata to file
    [[nodiscard]] std::error_code save(std::string_view path) const noexcept;

    // Load metadata from file
    [[nodiscard]] static std::expected<DownloadMeta, std::error_code>
    load(std::string_view path) noexcept;

    // Check if a .boltmeta file exists for the given output
    [[nodiscard]] static bool exists(std::string_view output_path) noexcept;

    // Delete the .boltmeta file
    static void remove(std::string_view output_path) noexcept;
};

} // namespace bolt::core
