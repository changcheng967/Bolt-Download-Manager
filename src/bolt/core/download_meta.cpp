// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/core/download_meta.hpp>
#include <bolt/core/error.hpp>
#include <bolt/disk/error.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace bolt::core {

namespace {

// Simple line-based format for metadata
// Lines: url, output_path, file_size, total_downloaded, segment_count
// Then per segment: id offset size file_offset downloaded

} // namespace

std::string DownloadMeta::meta_path(std::string_view output_path) {
    return std::string(output_path) + ".boltmeta";
}

std::error_code DownloadMeta::save(std::string_view path) const noexcept {
    try {
        // Create parent directories if they don't exist
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        std::ofstream file(std::string(path), std::ios::binary | std::ios::trunc);
        if (!file) {
            return make_error_code(disk::DiskErrc::write_error);
        }

        // Write header
        file << url << '\n';
        file << output_path << '\n';
        file << file_size << '\n';
        file << total_downloaded << '\n';
        file << segments.size() << '\n';

        // Write segments
        for (const auto& seg : segments) {
            file << seg.id << ' '
                 << seg.offset << ' '
                 << seg.size << ' '
                 << seg.file_offset << ' '
                 << seg.downloaded << '\n';
        }

        return {};
    } catch (...) {
        return make_error_code(disk::DiskErrc::write_error);
    }
}

std::expected<DownloadMeta, std::error_code>
DownloadMeta::load(std::string_view path) noexcept {
    try {
        std::ifstream file(std::string(path), std::ios::binary);
        if (!file) {
            return std::unexpected(make_error_code(disk::DiskErrc::file_not_found));
        }

        DownloadMeta meta;

        // Read header
        if (!std::getline(file, meta.url)) {
            return std::unexpected(make_error_code(DownloadErrc::invalid_url));
        }
        if (!std::getline(file, meta.output_path)) {
            return std::unexpected(make_error_code(DownloadErrc::invalid_url));
        }

        std::string line;
        if (!std::getline(file, line)) {
            return std::unexpected(make_error_code(DownloadErrc::invalid_range));
        }
        meta.file_size = std::stoull(line);

        if (!std::getline(file, line)) {
            return std::unexpected(make_error_code(DownloadErrc::invalid_range));
        }
        meta.total_downloaded = std::stoull(line);

        if (!std::getline(file, line)) {
            return std::unexpected(make_error_code(DownloadErrc::invalid_range));
        }
        std::size_t seg_count = std::stoull(line);

        // Read segments
        meta.segments.reserve(seg_count);
        for (std::size_t i = 0; i < seg_count; ++i) {
            if (!std::getline(file, line)) {
                return std::unexpected(make_error_code(DownloadErrc::invalid_range));
            }

            SegmentMeta seg;
            std::istringstream iss(line);
            iss >> seg.id >> seg.offset >> seg.size >> seg.file_offset >> seg.downloaded;
            meta.segments.push_back(seg);
        }

        return meta;
    } catch (...) {
        return std::unexpected(make_error_code(DownloadErrc::invalid_url));
    }
}

bool DownloadMeta::exists(std::string_view output_path) noexcept {
    return std::filesystem::exists(meta_path(output_path));
}

void DownloadMeta::remove(std::string_view output_path) noexcept {
    try {
        std::filesystem::remove(meta_path(output_path));
    } catch (...) {
        // Ignore errors on remove
    }
}

} // namespace bolt::core
