// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/core/bandwidth_prober.hpp>
#include <curl/curl.h>
#include <thread>
#include <algorithm>

namespace bolt::core {

BandwidthProber::BandwidthProber() = default;

BandwidthProber::BandwidthProber(Url target_url)
    : url_(std::move(target_url)) {}

BandwidthProber::ProbeResult BandwidthProber::probe(std::uint32_t duration_ms) noexcept {
    if (url_.str_.empty()) {
        // No URL set - return error
        return std::unexpected(make_error_code(DownloadErrc::no_bandwidth));
    }

    // Simple local probe - should be replaced with actual HTTP probe
    probing_.store(true, std::memory_order_release);
    cancelled_.store(false, std::memory_order_release);

    // Simulate measurement - in real implementation, download first ~1MB
    // and measure time taken
    std::uint64_t bandwidth = 10'000'000; // 10 MB/s default
    last_bandwidth_.store(bandwidth, std::memory_order_relaxed);

    probing_.store(false, std::memory_order_release);
    return bandwidth;
}

void BandwidthProber::probe_async(std::uint32_t duration_ms,
                                   std::function<void(ProbeResult)> callback) noexcept {
    std::thread([this, duration_ms, cb = std::move(callback)]() mutable {
        auto result = probe(duration_ms);
        if (cb) cb(result);
    }).detach();
}

void BandwidthProber::cancel() noexcept {
    cancelled_.store(true, std::memory_order_release);
}

//=============================================================================
// SegmentCalculator
//=============================================================================

SegmentCalculator::SegmentCalculator(std::uint64_t file_size) noexcept
    : file_size_(file_size) {}

std::uint32_t SegmentCalculator::optimal_segments(std::uint64_t bandwidth_bps) const noexcept {
    // For very high bandwidth, use fewer but larger segments
    if (bandwidth_bps >= HIGH_BANDWIDTH_THRESHOLD) {
        return MIN_SEGMENTS;
    }

    // For low bandwidth, use more segments to parallelize
    if (bandwidth_bps <= LOW_BANDWIDTH_THRESHOLD) {
        return MAX_SEGMENTS;
    }

    // Linear interpolation between min and max
    double ratio = static_cast<double>(bandwidth_bps - LOW_BANDWIDTH_THRESHOLD)
                 / (HIGH_BANDWIDTH_THRESHOLD - LOW_BANDWIDTH_THRESHOLD);
    return MIN_SEGMENTS + static_cast<std::uint32_t>(
        (MAX_SEGMENTS - MIN_SEGMENTS) * (1.0 - ratio));
}

std::uint64_t SegmentCalculator::optimal_segment_size(std::uint32_t segment_count) const noexcept {
    if (file_size_ == 0) return DEFAULT_SEGMENT_SIZE;

    // Distribute file across segments, but clamp to min/max
    std::uint64_t size = file_size_ / segment_count;
    return std::clamp(size, MIN_SEGMENT_SIZE, MAX_SEGMENT_SIZE);
}

bool SegmentCalculator::use_work_stealing(std::uint64_t avg_speed,
                                           std::uint64_t fast_speed,
                                           std::uint64_t slow_speed) const noexcept {
    // Only use work stealing if there's significant speed variance
    if (slow_speed == 0) return true;

    double variance = static_cast<double>(fast_speed - slow_speed) / fast_speed;
    return variance > SPEED_VARIANCE_THRESHOLD;
}

} // namespace bolt::core
