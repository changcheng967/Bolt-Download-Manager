// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/core/download_engine.hpp>
#include <bolt/core/config.hpp>
#include <algorithm>
#include <format>

namespace bolt::core {

//=============================================================================
// DownloadEngine
//=============================================================================

DownloadEngine::DownloadEngine() = default;

DownloadEngine::DownloadEngine(Url url)
    : url_(std::move(url))
    , seg_calculator_(std::make_unique<SegmentCalculator>()) {}

DownloadEngine::~DownloadEngine() {
    if (download_thread_.joinable()) {
        download_thread_.request_stop();
        download_thread_.join();
    }
}

std::expected<void, std::error_code> DownloadEngine::set_url(std::string_view url_str) noexcept {
    auto parsed = Url::parse(url_str);
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    return set_url(std::move(*parsed));
}

std::expected<void, std::error_code> DownloadEngine::set_url(Url url) noexcept {
    url_ = std::move(url);
    return {};
}

std::error_code DownloadEngine::prepare() noexcept {
    state_.store(DownloadState::preparing, std::memory_order_release);

    // Get file info via HEAD
    auto head_result = http_session_.head(url_.full());
    if (!head_result) {
        state_.store(DownloadState::failed, std::memory_order_release);
        return head_result.error();
    }

    server_info_ = *head_result;
    file_size_ = server_info_.content_length;
    supports_ranges_ = server_info_.accepts_ranges;
    content_type_ = server_info_.content_type;

    // Determine filename
    if (filename_.empty()) {
        filename_ = server_info_.filename.empty()
            ? url_.filename()
            : server_info_.filename;
    }

    // If output path not set, use filename
    if (output_path_.empty()) {
        output_path_ = filename_;
    }

    // Probe bandwidth
    prober_ = std::make_unique<BandwidthProber>(url_);
    auto bandwidth = prober_->probe();
    if (!bandwidth) {
        // Use default bandwidth on probe failure
        bandwidth = 10'000'000; // 10 MB/s default
    }

    seg_calculator_ = std::make_unique<SegmentCalculator>(file_size_);
    create_segments(*bandwidth);

    progress_.total_bytes = file_size_;
    progress_.start_time = std::chrono::steady_clock::now();
    progress_.last_update = progress_.start_time;

    return {};
}

void DownloadEngine::create_segments(std::uint64_t bandwidth_bps) noexcept {
    segments_.clear();

    if (!supports_ranges_ || file_size_ < MIN_SEGMENT_SIZE) {
        // Single segment for servers that don't support range requests
        segments_.push_back(std::make_unique<Segment>(
            0, url_, 0, file_size_, 0));
        return;
    }

    std::uint32_t seg_count = config_.auto_segment
        ? seg_calculator_->optimal_segments(bandwidth_bps)
        : config_.max_segments;

    std::uint64_t seg_size = seg_calculator_->optimal_segment_size(seg_count);
    std::uint64_t offset = 0;
    std::uint64_t file_offset = 0;

    std::uint32_t id = 0;
    while (offset < file_size_) {
        std::uint64_t this_size = std::min(seg_size, file_size_ - offset);
        segments_.push_back(std::make_unique<Segment>(
            id++, url_, offset, this_size, file_offset));
        offset += this_size;
        file_offset += this_size;
    }
}

std::error_code DownloadEngine::start() noexcept {
    if (state() != DownloadState::idle && state() != DownloadState::paused) {
        return make_error_code(DownloadErrc::network_error);
    }

    if (segments_.empty()) {
        auto prep_result = prepare();
        if (prep_result) {
            return prep_result;
        }
    }

    state_.store(DownloadState::downloading, std::memory_order_release);

    // Start download loop in background thread
    download_thread_ = std::jthread([this](std::stop_token stoken) {
        download_loop(stoken);
    });

    return {};
}

void DownloadEngine::pause() noexcept {
    auto current = state().load(std::memory_order_acquire);
    if (current == DownloadState::downloading) {
        state_.store(DownloadState::paused, std::memory_order_release);
        download_thread_.request_stop();
    }
}

std::error_code DownloadEngine::resume() noexcept {
    if (state() != DownloadState::paused) {
        return make_error_code(DownloadErrc::network_error);
    }

    state_.store(DownloadState::downloading, std::memory_order_release);

    // Restart stalled segments
    for (auto& seg : segments_) {
        if (seg->state() == SegmentState::stalled) {
            seg->resume();
        }
    }

    // Restart download thread if needed
    if (!download_thread_.joinable()) {
        download_thread_ = std::jthread([this](std::stop_token stoken) {
            download_loop(stoken);
        });
    }

    return {};
}

void DownloadEngine::cancel() noexcept {
    state_.store(DownloadState::cancelled, std::memory_order_release);
    download_thread_.request_stop();

    for (auto& seg : segments_) {
        seg->cancel();
    }
}

DownloadProgress DownloadEngine::progress() const noexcept {
    auto lock = std::unique_lock(mutex_);
    return progress_;
}

std::vector<SegmentProgress> DownloadEngine::segment_progress() const noexcept {
    std::vector<SegmentProgress> result;
    result.reserve(segments_.size());

    auto lock = std::unique_lock(mutex_);
    for (const auto& seg : segments_) {
        auto seg_lock = seg->lock();
        result.push_back(seg->progress());
    }

    return result;
}

void DownloadEngine::download_loop(std::stop_token stoken) noexcept {
    using namespace std::chrono_literals;

    // Start all segments
    for (auto& seg : segments_) {
        if (seg->state() == SegmentState::pending) {
            seg->start();
        }
    }

    // Monitor progress
    while (!stoken.stop_requested() && state() == DownloadState::downloading) {
        update_progress();
        monitor_segments();

        if (config_.work_stealing) {
            attempt_work_stealing();
        }

        // Check completion
        std::uint32_t completed = 0;
        std::uint32_t failed = 0;

        for (const auto& seg : segments_) {
            auto s = seg->state();
            if (s == SegmentState::completed) ++completed;
            if (s == SegmentState::failed) ++failed;
        }

        progress_.completed_segments = completed;
        progress_.failed_segments = failed;

        if (completed == segments_.size()) {
            state_.store(DownloadState::completed, std::memory_order_release);
            update_progress();
            if (callback_) callback_(progress_);
            break;
        }

        if (failed > 0 && completed + failed == segments_.size()) {
            state_.store(DownloadState::failed, std::memory_order_release);
            update_progress();
            if (callback_) callback_(progress_);
            break;
        }

        if (callback_) callback_(progress_);

        std::this_thread::sleep_for(BANDWIDTH_SAMPLE_INTERVAL);
    }
}

void DownloadEngine::monitor_segments() noexcept {
    using namespace std::chrono_literals;

    for (auto& seg : segments_) {
        if (seg->is_stalled(std::chrono::seconds{STALL_TIMEOUT_SEC})) {
            seg->state(SegmentState::stalled);
            // TODO: Restart stalled segment
        }
    }
}

void DownloadEngine::attempt_work_stealing() noexcept {
    if (segments_.size() < 2) return;

    for (auto& requester : segments_) {
        if (requester->state() != SegmentState::downloading) continue;

        auto lock = requester->lock();
        const auto& req_progress = requester->progress();

        // If this segment is much slower than others, try to steal
        // This is a simplified check - real implementation would be smarter
        if (req_progress.speed_bps < 100'000) { // < 100 KB/s
            auto steal_result = find_steal_target(segments_, requester->id(), 1'000'000);
            if (steal_result) {
                auto [target_id, bytes] = *steal_result;
                // Transfer bytes from target to requester
                for (auto& target : segments_) {
                    if (target->id() == target_id) {
                        target->steal_bytes(bytes);
                        requester->steal_bytes(-static_cast<std::int64_t>(bytes)); // Add to requester
                        break;
                    }
                }
            }
        }
    }
}

void DownloadEngine::update_progress() noexcept {
    auto lock = std::unique_lock(mutex_);

    std::uint64_t total_downloaded = 0;
    std::uint64_t total_speed = 0;
    std::uint32_t active = 0;

    for (const auto& seg : segments_) {
        total_downloaded += seg->downloaded();
        total_speed += seg->progress().speed_bps;
        if (seg->state() == SegmentState::downloading) ++active;
    }

    progress_.downloaded_bytes = total_downloaded;
    progress_.speed_bps = total_speed;
    progress_.active_segments = active;
    progress_.last_update = std::chrono::steady_clock::now();

    if (progress_.total_bytes > 0) {
        progress_.percent = static_cast<double>(total_downloaded) * 100.0
                          / static_cast<double>(progress_.total_bytes);
    }

    // Calculate average speed
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        progress_.last_update - progress_.start_time).count();
    if (elapsed > 0) {
        progress_.average_speed_bps = total_downloaded / elapsed;
    }

    calculate_eta();
}

void DownloadEngine::calculate_eta() noexcept {
    if (progress_.speed_bps == 0) {
        progress_.eta_seconds = 0;
        return;
    }

    std::uint64_t remaining = progress_.total_bytes - progress_.downloaded_bytes;
    progress_.eta_seconds = remaining / progress_.speed_bps;
}

void DownloadEngine::reset() noexcept {
    segments_.clear();
    progress_ = {};
    total_downloaded_.store(0, std::memory_order_release);
    state_.store(DownloadState::idle, std::memory_order_release);
}

//=============================================================================
// DownloadManager
//=============================================================================

std::expected<std::uint32_t, std::error_code>
DownloadManager::create_download(std::string_view url, std::string_view output_path) noexcept {
    auto lock = std::unique_lock(mutex_);

    std::uint32_t id = next_id_++;
    auto engine = std::make_unique<DownloadEngine>();

    auto result = engine->set_url(url);
    if (!result) {
        return std::unexpected(result.error());
    }

    if (!output_path.empty()) {
        engine->output_path(std::string(output_path));
    }

    downloads_[id] = std::move(engine);
    return id;
}

std::error_code DownloadManager::start(std::uint32_t id) noexcept {
    auto lock = std::unique_lock(mutex_);
    auto it = downloads_.find(id);
    if (it == downloads_.end()) {
        return make_error_code(DownloadErrc::invalid_url);
    }
    return it->second->start();
}

void DownloadManager::pause(std::uint32_t id) noexcept {
    auto lock = std::unique_lock(mutex_);
    auto it = downloads_.find(id);
    if (it != downloads_.end()) {
        it->second->pause();
    }
}

std::error_code DownloadManager::resume(std::uint32_t id) noexcept {
    auto lock = std::unique_lock(mutex_);
    auto it = downloads_.find(id);
    if (it == downloads_.end()) {
        return make_error_code(DownloadErrc::invalid_url);
    }
    return it->second->resume();
}

void DownloadManager::cancel(std::uint32_t id) noexcept {
    auto lock = std::unique_lock(mutex_);
    auto it = downloads_.find(id);
    if (it != downloads_.end()) {
        it->second->cancel();
    }
}

void DownloadManager::remove(std::uint32_t id) noexcept {
    auto lock = std::unique_lock(mutex_);
    auto it = downloads_.find(id);
    if (it != downloads_.end()) {
        auto state = it->second->state();
        if (state == DownloadState::completed ||
            state == DownloadState::failed ||
            state == DownloadState::cancelled) {
            downloads_.erase(it);
        }
    }
}

std::expected<DownloadProgress, std::error_code>
DownloadManager::progress(std::uint32_t id) const noexcept {
    auto lock = std::unique_lock(mutex_);
    auto it = downloads_.find(id);
    if (it == downloads_.end()) {
        return std::unexpected(make_error_code(DownloadErrc::invalid_url));
    }
    return it->second->progress();
}

std::vector<std::uint32_t> DownloadManager::downloads() const noexcept {
    auto lock = std::unique_lock(mutex_);
    std::vector<std::uint32_t> result;
    result.reserve(downloads_.size());
    for (const auto& [id, _] : downloads_) {
        result.push_back(id);
    }
    return result;
}

} // namespace bolt::core
