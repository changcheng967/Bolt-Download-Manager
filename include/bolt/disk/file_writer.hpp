// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/disk/async_file.hpp>
#include <bolt/disk/error.hpp>
#include <cstdint>
#include <vector>
#include <expected>
#include <mutex>
#include <atomic>

namespace bolt::disk {

// Write buffer size for coalescing
constexpr std::size_t WRITE_BUFFER_SIZE = 256 * 1024; // 256 KB

// Thread-safe file writer for concurrent segment writes
class FileWriter {
public:
    FileWriter() = default;

    // Open file for writing
    [[nodiscard]] std::error_code open(std::string_view path, std::uint64_t size) noexcept;

    // Write data at offset (thread-safe)
    [[nodiscard]] std::error_code write(std::uint64_t offset,
                                         const void* data,
                                         std::size_t size) noexcept;

    // Queue async write
    void write_async(std::uint64_t offset,
                              std::vector<std::byte> data,
                              std::function<void(std::error_code)> callback) noexcept;

    // Flush all pending writes
    [[nodiscard]] std::error_code flush() noexcept;

    // Close file
    void close() noexcept;

    // Check if file is open
    [[nodiscard]] bool is_open() const noexcept { return file_ != nullptr; }

    // Get file path
    [[nodiscard]] const std::string& path() const noexcept { return path_; }

private:
    std::unique_ptr<AsyncFile> file_;
    std::string path_;
    std::atomic<bool> closed_{false};  // Guard against double-close
};

// RAII buffer for segment data
class SegmentBuffer {
public:
    explicit SegmentBuffer(std::size_t capacity = WRITE_BUFFER_SIZE);

    [[nodiscard]] void* data() noexcept { return buffer_.data(); }
    [[nodiscard]] const void* data() const noexcept { return buffer_.data(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return buffer_.size(); }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    void size(std::size_t s) noexcept { size_ = s; }
    void reset() noexcept { size_ = 0; }

    // Append data
    void append(const void* data, std::size_t size) noexcept;

    // Reserve capacity
    void reserve(std::size_t capacity) noexcept { buffer_.resize(capacity); }

private:
    std::vector<std::byte> buffer_;
    std::size_t size_{0};
};

} // namespace bolt::disk
