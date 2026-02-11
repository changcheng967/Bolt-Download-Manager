// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/disk/async_file.hpp>
#include <cstdint>
#include <vector>
#include <map>
#include <mutex>
#include <memory>

namespace bolt::disk {

// Coalesces adjacent writes for better disk I/O performance
class WriteCoalescer {
public:
    explicit WriteCoalescer(std::uint64_t max_pending = 16 * 1024 * 1024); // 16 MB

    // Add write to coalescer queue
    void enqueue(std::uint64_t offset,
                 std::vector<std::byte> data) noexcept;

    // Flush all pending writes to file
    [[nodiscard]] std::error_code flush(AsyncFile& file) noexcept;

    // Cancel all pending writes
    void cancel() noexcept;

    // Get total pending bytes
    [[nodiscard]] std::uint64_t pending_bytes() const noexcept;

    // Get pending write count
    [[nodiscard]] std::size_t pending_count() const noexcept;

private:
    struct PendingWrite {
        std::uint64_t offset;
        std::vector<std::byte> data;
    };

    // Merge adjacent/overlapping writes
    void merge_writes() noexcept;

    std::map<std::uint64_t, PendingWrite> pending_;
    std::uint64_t max_pending_;
    std::uint64_t total_pending_{0};
    mutable std::mutex mutex_;
};

} // namespace bolt::disk
