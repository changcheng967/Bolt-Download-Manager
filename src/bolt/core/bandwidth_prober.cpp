// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/core/bandwidth_prober.hpp>
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <algorithm>

namespace bolt::core {

namespace {

// Write callback that discards data (we only care about speed measurement)
std::size_t discard_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) noexcept {
    auto* total = static_cast<std::uint64_t*>(userdata);
    std::size_t bytes = size * nmemb;
    *total += bytes;
    return bytes;
}

// Progress callback to check cancellation
int probe_progress_callback(void* userdata, curl_off_t dltotal, curl_off_t dlnow,
                            curl_off_t ultotal, curl_off_t ulnow) noexcept {
    (void)dltotal; (void)dlnow; (void)ultotal; (void)ulnow;
    auto* cancelled = static_cast<std::atomic<bool>*>(userdata);
    return cancelled->load(std::memory_order_relaxed) ? 1 : 0;
}

} // namespace

BandwidthProber::BandwidthProber() = default;

BandwidthProber::BandwidthProber(Url target_url)
    : url_(std::move(target_url)) {}

BandwidthProber::ProbeResult BandwidthProber::probe(std::uint32_t duration_ms) noexcept {
    if (url_.full().empty()) {
        return std::unexpected(make_error_code(DownloadErrc::no_bandwidth));
    }

    probing_.store(true, std::memory_order_release);
    cancelled_.store(false, std::memory_order_release);

    CURL* curl = curl_easy_init();
    if (!curl) {
        probing_.store(false, std::memory_order_release);
        return std::unexpected(make_error_code(DownloadErrc::network_error));
    }

    // Set up curl for bandwidth probing
    std::uint64_t bytes_downloaded = 0;
    curl_easy_setopt(curl, CURLOPT_URL, url_.full().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bytes_downloaded);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, probe_progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancelled_);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);  // 10 second max
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    // Download only first 512KB for speed measurement
    curl_easy_setopt(curl, CURLOPT_RANGE, "0-524287");

    auto start_time = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end_time = std::chrono::steady_clock::now();

    curl_easy_cleanup(curl);

    if (res != CURLE_OK && res != CURLE_WRITE_ERROR) {
        probing_.store(false, std::memory_order_release);
        return std::unexpected(make_error_code(DownloadErrc::network_error));
    }

    // Calculate bandwidth
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    std::uint64_t bandwidth = 0;
    if (duration_ns > 0 && bytes_downloaded > 0) {
        // bandwidth = (bytes * 1e9) / nanoseconds = bytes/second
        bandwidth = (bytes_downloaded * 1'000'000'000ULL) / static_cast<std::uint64_t>(duration_ns);
    }

    // If bandwidth is unreasonably low, use a minimum default
    if (bandwidth < 100'000) {  // < 100 KB/s
        bandwidth = 1'000'000;  // Default to 1 MB/s
    }

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
    // For very high bandwidth, use MORE segments to saturate the connection
    if (bandwidth_bps >= HIGH_BANDWIDTH_THRESHOLD) {
        return MAX_SEGMENTS;
    }

    // For low bandwidth, use fewer segments to reduce overhead
    if (bandwidth_bps <= LOW_BANDWIDTH_THRESHOLD) {
        return MIN_SEGMENTS;
    }

    // Linear interpolation: more bandwidth = more segments
    double ratio = static_cast<double>(bandwidth_bps - LOW_BANDWIDTH_THRESHOLD)
                 / (HIGH_BANDWIDTH_THRESHOLD - LOW_BANDWIDTH_THRESHOLD);
    return MIN_SEGMENTS + static_cast<std::uint32_t>(
        (MAX_SEGMENTS - MIN_SEGMENTS) * ratio);
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
