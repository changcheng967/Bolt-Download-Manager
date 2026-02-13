// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/core/error.hpp>
#include <bolt/core/url.hpp>
#include <bolt/core/segment.hpp>
#include <bolt/core/http_session.hpp>
#include <bolt/core/bandwidth_prober.hpp>
#include <bolt/core/download_meta.hpp>
#include <bolt/disk/file_writer.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <expected>
#include <thread>
#include <stop_token>

namespace bolt::core {

// Overall download state
enum class DownloadState : std::uint8_t {
    idle,        // Not started
    preparing,   // Probing bandwidth, setting up segments
    downloading, // Actively downloading
    paused,      // Paused by user
    stalled,     // All segments stalled
    completing,  // Finishing up
    completed,   // All done
    failed,      // Failed with error
    cancelled    // Cancelled by user
};

// Overall download progress
struct DownloadProgress {
    std::uint64_t total_bytes{0};
    std::uint64_t downloaded_bytes{0};
    std::uint64_t speed_bps{0};           // Current total speed
    std::uint64_t average_speed_bps{0};   // Average since start
    std::uint32_t active_segments{0};
    std::uint32_t completed_segments{0};
    std::uint32_t failed_segments{0};
    double percent{0.0};
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_update;
    std::uint64_t eta_seconds{0};         // Estimated time remaining
};

// Download configuration
struct DownloadConfig {
    // Segment limits
    static constexpr std::uint32_t MAX_SEGMENTS = 16;
    static constexpr std::uint32_t MIN_SEGMENTS = 2;
    static constexpr std::uint64_t DEFAULT_SEGMENT_SIZE = 5'000'000;  // 5 MB
    static constexpr std::uint32_t IO_TIMEOUT_SEC = 30;
    static constexpr std::uint32_t RETRY_COUNT = 3;

    std::uint32_t max_segments{MAX_SEGMENTS};
    std::uint32_t min_segments{MIN_SEGMENTS};
    std::uint64_t segment_size{DEFAULT_SEGMENT_SIZE};
    bool auto_segment{true};              // Auto-adjust segment count
    bool work_stealing{true};             // Enable work stealing
    bool use_http2{true};                 // Prefer HTTP/2
};

// Download event callback
using DownloadCallback = std::function<void(const DownloadProgress&)>;

// Main download engine
class DownloadEngine {
public:
    DownloadEngine();
    explicit DownloadEngine(Url url);
    ~DownloadEngine();

    // Non-copyable, non-movable (atomic members can't be moved)
    DownloadEngine(const DownloadEngine&) = delete;
    DownloadEngine& operator=(const DownloadEngine&) = delete;
    DownloadEngine(DownloadEngine&&) noexcept = delete;
    DownloadEngine& operator=(DownloadEngine&&) noexcept = delete;

    // Set URL and start preparing download
    [[nodiscard]] std::expected<void, std::error_code> set_url(std::string_view url_str) noexcept;
    [[nodiscard]] std::expected<void, std::error_code> set_url(Url url) noexcept;

    // Start the download
    [[nodiscard]] std::error_code start() noexcept;

    // Pause the download
    void pause() noexcept;

    // Resume the download
    [[nodiscard]] std::error_code resume() noexcept;

    // Cancel the download
    void cancel() noexcept;

    // Set output path
    void output_path(std::string path) noexcept { output_path_ = std::move(path); }
    [[nodiscard]] const std::string& output_path() const noexcept { return output_path_; }

    // Set configuration
    void config(const DownloadConfig& cfg) noexcept { config_ = cfg; }
    [[nodiscard]] const DownloadConfig& config() const noexcept { return config_; }

    // Set progress callback (thread-safe)
    void callback(DownloadCallback cb) noexcept {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback_ = std::move(cb);
    }

    // Get current state
    [[nodiscard]] DownloadState state() const noexcept { return state_.load(std::memory_order_acquire); }

    // Get progress (thread-safe snapshot)
    [[nodiscard]] DownloadProgress progress() const noexcept;

    // Get file info
    [[nodiscard]] std::uint64_t file_size() const noexcept { return file_size_; }
    [[nodiscard]] const std::string& filename() const noexcept { return filename_; }
    [[nodiscard]] const std::string& content_type() const noexcept { return content_type_; }

    // Get individual segment progress (for UI)
    [[nodiscard]] std::vector<SegmentProgress> segment_progress() const noexcept;

    // Global initialization
    static void global_init() noexcept { HttpSession::global_init(); }
    static void global_cleanup() noexcept { HttpSession::global_cleanup(); }

private:
    // Prepare download (probe bandwidth, create segments)
    [[nodiscard]] std::error_code prepare() noexcept;

    // Create segments based on bandwidth
    void create_segments(std::uint64_t bandwidth_bps) noexcept;

    // Main download loop (runs in thread)
    void download_loop(std::stop_token stoken) noexcept;

    // Monitor segment health and handle stalls
    void monitor_segments() noexcept;

    // Perform work stealing if needed
    void attempt_work_stealing() noexcept;

    // Update overall progress
    void update_progress() noexcept;

    // Calculate ETA
    void calculate_eta() noexcept;

    // Reset for new download
    void reset() noexcept;

    // Stop the download thread
    void stop_download() noexcept;

    // Metadata persistence for resume
    [[nodiscard]] std::error_code save_meta() const noexcept;
    void delete_meta() const noexcept;

    Url url_;
    std::string output_path_;
    DownloadConfig config_;

    std::atomic<DownloadState> state_{DownloadState::idle};
    DownloadProgress progress_;
    std::atomic<std::uint64_t> total_downloaded_{0};

    std::uint64_t file_size_{0};
    std::string filename_;
    std::string content_type_;
    bool supports_ranges_{true};
    HttpResponse server_info_;

    std::vector<std::unique_ptr<Segment>> segments_;
    std::unique_ptr<BandwidthProber> prober_;
    std::unique_ptr<SegmentCalculator> seg_calculator_;

    DownloadCallback callback_;
    std::mutex callback_mutex_;  // Protects callback_ access
    HttpSession http_session_;
    bolt::disk::FileWriter file_writer_;

    std::jthread download_thread_;
    mutable std::mutex mutex_;
};

// Factory for creating download engines
class DownloadManager {
public:
    static DownloadManager& instance() noexcept {
        static DownloadManager mgr;
        return mgr;
    }

    // Create a new download
    [[nodiscard]] std::expected<std::uint32_t, std::error_code>
    create_download(std::string_view url, std::string_view output_path = {}) noexcept;

    // Start a download by ID
    [[nodiscard]] std::error_code start(std::uint32_t id) noexcept;

    // Pause a download
    void pause(std::uint32_t id) noexcept;

    // Resume a download
    [[nodiscard]] std::error_code resume(std::uint32_t id) noexcept;

    // Cancel a download
    void cancel(std::uint32_t id) noexcept;

    // Remove a download (must be completed/failed/cancelled)
    void remove(std::uint32_t id) noexcept;

    // Get download progress
    [[nodiscard]] std::expected<DownloadProgress, std::error_code> progress(std::uint32_t id) const noexcept;

    // Get all download IDs
    [[nodiscard]] std::vector<std::uint32_t> downloads() const noexcept;

private:
    DownloadManager() = default;
    ~DownloadManager() = default;

    std::map<std::uint32_t, std::unique_ptr<DownloadEngine>> downloads_;
    std::uint32_t next_id_{1};
    mutable std::mutex mutex_;
};

} // namespace bolt::core
