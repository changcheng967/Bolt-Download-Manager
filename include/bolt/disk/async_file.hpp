// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/core/error.hpp>
#include <bolt/disk/error.hpp>
#include <cstdint>
#include <string>
#include <expected>
#include <functional>
#include <memory>

// Windows overlapped I/O
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace bolt::disk {

// Async operation result
using AsyncCallback = std::function<void(std::uint64_t bytes_transferred, std::error_code ec)>;

// Async file handle wrapper
class AsyncFile {
public:
    // Open file for async write operations
    static std::expected<AsyncFile, std::error_code>
    open(std::string_view path, std::uint64_t size = 0) noexcept;

    ~AsyncFile();

    // Non-copyable, movable
    AsyncFile(const AsyncFile&) = delete;
    AsyncFile& operator=(const AsyncFile&) = delete;
    AsyncFile(AsyncFile&&) noexcept;
    AsyncFile& operator=(AsyncFile&&) noexcept;

    // Pre-allocate file space (sparse file support)
    [[nodiscard]] std::error_code pre_allocate(std::uint64_t size) noexcept;

    // Async write at offset
    void async_write(std::uint64_t offset,
                     const void* data,
                     std::size_t size,
                     AsyncCallback callback) noexcept;

    // Async read at offset
    void async_read(std::uint64_t offset,
                    void* buffer,
                    std::size_t size,
                    AsyncCallback callback) noexcept;

    // Sync write (for simplicity during development)
    [[nodiscard]] std::expected<std::size_t, std::error_code>
    write(std::uint64_t offset, const void* data, std::size_t size) noexcept;

    // Sync read
    [[nodiscard]] std::expected<std::size_t, std::error_code>
    read(std::uint64_t offset, void* buffer, std::size_t size) noexcept;

    // Flush buffers to disk
    [[nodiscard]] std::error_code flush() noexcept;

    // Close file
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return handle_ != INVALID_HANDLE_VALUE; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }

private:
    AsyncFile() = default;

    HANDLE handle_{INVALID_HANDLE_VALUE};
    std::string path_;
};

// Memory-mapped file for very large files (alternative approach)
class MappedFile {
public:
    static std::expected<MappedFile, std::error_code>
    create(std::string_view path, std::uint64_t size) noexcept;

    ~MappedFile();

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&&) noexcept;
    MappedFile& operator=(MappedFile&&) noexcept;

    // Write data at offset
    [[nodiscard]] std::error_code write(std::uint64_t offset,
                                        const void* data,
                                        std::size_t size) noexcept;

    // Read data at offset
    [[nodiscard]] std::error_code read(std::uint64_t offset,
                                       void* buffer,
                                       std::size_t size) noexcept;

    // Flush to disk
    [[nodiscard]] std::error_code flush() noexcept;

    void close() noexcept;

private:
    MappedFile() = default;

    HANDLE file_{INVALID_HANDLE_VALUE};
    HANDLE mapping_{nullptr};
    void* data_{nullptr};
    std::uint64_t size_{0};
    std::string path_;
};

} // namespace bolt::disk
