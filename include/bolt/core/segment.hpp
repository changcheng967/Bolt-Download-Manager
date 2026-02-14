// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/core/error.hpp>
#include <bolt/core/url.hpp>
#include <bolt/disk/file_writer.hpp>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <functional>
#include <expected>
#include <vector>
#include <thread>

namespace bolt::core {

// Segment state machine
enum class SegmentState : std::uint8_t {
    pending,     // Not started
    connecting,  // Establishing connection
    downloading, // Actively downloading
    stalled,     // No progress for too long
    completed,   // Finished successfully
    failed,      // Failed with error
    cancelled    // Cancelled by user
};

// Progress information for a segment
struct SegmentProgress {
    std::uint64_t downloaded_bytes{0};
    std::uint64_t total_bytes{0};
    std::uint64_t speed_bps{0};           // Current speed
    std::uint64_t average_speed_bps{0};   // Average since start
    std::chrono::steady_clock::time_point last_update;
    std::chrono::steady_clock::time_point start_time;
};

// A single download segment (byte range)
class Segment {
public:
    Segment(std::uint32_t id, Url url,
            std::uint64_t offset, std::uint64_t size,
            std::uint64_t file_offset) noexcept;

    // Non-copyable, movable
    Segment(const Segment&) = delete;
    Segment& operator=(const Segment&) = delete;
    Segment(Segment&& other) noexcept;
    Segment& operator=(Segment&& other) noexcept;

    // Start downloading this segment
    [[nodiscard]] std::error_code start() noexcept;

    // Pause the segment
    void pause() noexcept;

    // Resume the segment
    [[nodiscard]] std::error_code resume() noexcept;

    // Cancel the segment
    void cancel() noexcept;

    // Check if segment is stalled (no progress for STALL_TIMEOUT)
    [[nodiscard]] bool is_stalled(std::chrono::seconds timeout) const noexcept;

    // Steal work from another segment (split remaining bytes)
    [[nodiscard]] std::uint64_t can_steal(std::uint64_t min_steal) const noexcept;

    // Add stolen bytes (for work stealing receiver)
    void add_bytes(std::uint64_t bytes) noexcept;

    // Remove bytes (for work stealing sender)
    void steal_bytes(std::uint64_t bytes) noexcept;

    // Getters
    [[nodiscard]] std::uint32_t id() const noexcept { return id_; }
    [[nodiscard]] SegmentState state() const noexcept { return state_.load(std::memory_order_acquire); }
    [[nodiscard]] std::uint64_t offset() const noexcept { return offset_; }
    [[nodiscard]] std::uint64_t size() const noexcept { return size_; }
    [[nodiscard]] std::uint64_t file_offset() const noexcept { return file_offset_; }
    [[nodiscard]] std::uint64_t downloaded() const noexcept { return atomic_downloaded_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::uint64_t write_offset() const noexcept { return atomic_write_offset_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::uint64_t remaining() const noexcept;
    [[nodiscard]] double percent() const noexcept;

    // Access progress (returns copy, may be slightly stale - that's OK)
    [[nodiscard]] SegmentProgress progress() const noexcept {
        // No lock - values may be slightly stale but that's acceptable
        SegmentProgress copy = progress_;
        copy.downloaded_bytes = atomic_downloaded_.load(std::memory_order_relaxed);
        return copy;
    }

    // Update downloaded bytes (called by write callback)
    void add_downloaded(std::uint64_t bytes) noexcept;

    // Set initial downloaded bytes (for resume)
    void set_downloaded(std::uint64_t bytes) noexcept {
        atomic_downloaded_.store(bytes, std::memory_order_relaxed);
        atomic_write_offset_.store(bytes, std::memory_order_relaxed);
    }

    // Set file writer for writing downloaded data
    void file_writer(bolt::disk::FileWriter* writer) noexcept { file_writer_ = writer; }
    [[nodiscard]] bolt::disk::FileWriter* file_writer() const noexcept { return file_writer_; }

    // Set new state
    void state(SegmentState new_state) noexcept { state_.store(new_state, std::memory_order_release); }

    // Last error
    [[nodiscard]] const std::error_code& error() const noexcept { return error_; }
    void error(std::error_code ec) noexcept { error_ = ec; }

    // Check if stop was requested (for curl progress callback)
    [[nodiscard]] bool stop_requested() const noexcept { return stop_requested_.load(std::memory_order_acquire); }

private:
    std::uint32_t id_;
    Url url_;
    std::uint64_t offset_;       // HTTP Range start
    std::uint64_t size_;         // Total segment size
    std::uint64_t file_offset_;  // Write position in output file
    std::atomic<SegmentState> state_{SegmentState::pending};
    SegmentProgress progress_;
    std::error_code error_;

    // Atomic counters for thread-safe updates from callback (no mutex needed)
    std::atomic<std::uint64_t> atomic_downloaded_{0};
    std::atomic<std::uint64_t> atomic_write_offset_{0};
    std::atomic<std::uint64_t> atomic_speed_bytes_{0};  // Accumulated bytes for speed calc

    void* curl_handle_{nullptr};  // CURL* handle
    bolt::disk::FileWriter* file_writer_{nullptr};  // File writer for saving data
    std::jthread segment_thread_;  // Thread for async download
    std::atomic<bool> stop_requested_{false};  // Signal for curl progress callback
};

// Work stealing between segments
std::expected<std::pair<std::uint32_t, std::uint64_t>, std::error_code>
find_steal_target(const std::vector<std::unique_ptr<Segment>>& segments,
                   std::uint32_t requester_id,
                   std::uint64_t min_bytes) noexcept;

} // namespace bolt::core
