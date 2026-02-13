// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/core/error.hpp>
#include <bolt/core/url.hpp>
#include <bolt/core/config.hpp>
#include <cstdint>
#include <chrono>
#include <atomic>
#include <functional>
#include <expected>
#include <vector>

namespace bolt::core {

class BandwidthProber {
public:
    using ProbeResult = std::expected<std::uint64_t, std::error_code>; // bytes per second

    BandwidthProber();
    explicit BandwidthProber(Url target_url);

    // Probe available bandwidth by downloading small chunks
    [[nodiscard]] ProbeResult probe(std::uint32_t duration_ms = 2000) noexcept;

    // Async probe with callback
    void probe_async(std::uint32_t duration_ms,
                     std::function<void(ProbeResult)> callback) noexcept;

    // Cancel ongoing probe
    void cancel() noexcept;

    [[nodiscard]] std::uint64_t last_bandwidth() const noexcept { return last_bandwidth_.load(std::memory_order_relaxed); }
    [[nodiscard]] bool is_probing() const noexcept { return probing_.load(std::memory_order_relaxed); }

private:
    Url url_;
    std::atomic<std::uint64_t> last_bandwidth_{0};
    std::atomic<bool> probing_{false};
    std::atomic<bool> cancelled_{false};
};

// Adaptive segment calculator based on measured bandwidth
class SegmentCalculator {
public:
    explicit SegmentCalculator(std::uint64_t file_size = 0) noexcept;

    // Calculate optimal segment count based on bandwidth
    [[nodiscard]] std::uint32_t optimal_segments(std::uint64_t bandwidth_bps) const noexcept;

    // Calculate optimal segment size
    [[nodiscard]] std::uint64_t optimal_segment_size(std::uint32_t segment_count) const noexcept;

    // Update file size
    void file_size(std::uint64_t size) noexcept { file_size_ = size; }

    // Should we use work stealing?
    [[nodiscard]] bool use_work_stealing(std::uint64_t avg_speed,
                                          std::uint64_t fast_speed,
                                          std::uint64_t slow_speed) const noexcept;

private:
    std::uint64_t file_size_;
    static constexpr std::uint64_t HIGH_BANDWIDTH_THRESHOLD = 100'000'000; // 100 MB/s
    static constexpr std::uint64_t LOW_BANDWIDTH_THRESHOLD = 1'000'000;      // 1 MB/s
    static constexpr double SPEED_VARIANCE_THRESHOLD = 0.5;                 // 50% difference
};

} // namespace bolt::core
