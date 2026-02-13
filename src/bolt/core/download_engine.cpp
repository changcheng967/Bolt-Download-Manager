// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/core/download_engine.hpp>
#include <bolt/core/config.hpp>
#include <algorithm>

namespace bolt::core {

//=============================================================================
// DownloadEngine
//=============================================================================

DownloadEngine::DownloadEngine() = default;

DownloadEngine::DownloadEngine(Url url)
    : url_(std::move(url))
    , seg_calculator_(std::make_unique<SegmentCalculator>()) {}

DownloadEngine::~DownloadEngine() {
    // 1. Stop and join download loop thread FIRST
    // This is critical: download_loop iterates over segments_, so must be
    // stopped before we touch segments. Also prevents race where download_loop
    // accesses segments while they're being cancelled.
    if (download_thread_.joinable()) {
        download_thread_.request_stop();
        download_thread_.join();
    }

    // 2. NOW cancel all segments (stops curl transfers and joins their threads)
    // download_loop is no longer running, so no race on segments_ access
    for (auto& seg : segments_) {
        if (seg) seg->cancel();
    }

    // 3. All threads are now done. Safe to close file.
    if (file_writer_.is_open()) {
        (void)file_writer_.flush();
        file_writer_.close();
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

    // Handle unknown file size (server may use chunked encoding)
    if (file_size_ == 0) {
        // Unknown size - disable multi-segment and use streaming download
        supports_ranges_ = false;
    }

    // Probe bandwidth - disabled for now, use hardcoded value
    // TODO: Implement proper bandwidth probing
    std::uint64_t bandwidth = 10'000'000; // 10 MB/s default

    seg_calculator_ = std::make_unique<SegmentCalculator>(file_size_);

    // Open output file for writing (0 size means grow as needed)
    auto open_result = file_writer_.open(output_path_, file_size_);
    if (open_result) {
        state_.store(DownloadState::failed, std::memory_order_release);
        return open_result;
    }

    create_segments(bandwidth);

    progress_.total_bytes = file_size_;
    progress_.start_time = std::chrono::steady_clock::now();
    progress_.last_update = progress_.start_time;

    return {};
}

void DownloadEngine::create_segments(std::uint64_t bandwidth_bps) noexcept {
    segments_.clear();

    if (!supports_ranges_ || file_size_ < MIN_SEGMENT_SIZE) {
        // Single segment for servers that don't support range requests
        auto seg = std::make_unique<Segment>(0, url_, 0, file_size_, 0);
        seg->file_writer(&file_writer_);
        segments_.push_back(std::move(seg));
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
        auto seg = std::make_unique<Segment>(id++, url_, offset, this_size, file_offset);
        seg->file_writer(&file_writer_);
        segments_.push_back(std::move(seg));
        offset += this_size;
        file_offset += this_size;
    }
}

std::error_code DownloadEngine::start() noexcept {
    // Only allow starting if not already downloading, completed, failed, or cancelled
    DownloadState current_state = state();

    if (current_state == DownloadState::downloading ||
        current_state == DownloadState::completed ||
        current_state == DownloadState::failed ||
        current_state == DownloadState::cancelled) {
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
    auto current = state_.load(std::memory_order_acquire);
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
            (void)seg->resume();
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

    // 1. Stop and join download loop thread FIRST
    // Prevents race where download_loop accesses segments while we cancel them
    if (download_thread_.joinable()) {
        download_thread_.request_stop();
        download_thread_.join();
    }

    // 2. NOW cancel all segments (signals curl to abort and joins their threads)
    // download_loop is no longer running, so no race
    for (auto& seg : segments_) {
        if (seg) seg->cancel();
    }

    // 3. Small delay to ensure any in-flight callbacks complete
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 4. Close file (all threads now done)
    if (file_writer_.is_open()) {
        (void)file_writer_.flush();
        file_writer_.close();
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
        // progress() now returns a copy, no need to lock segment
        result.push_back(seg->progress());
    }

    return result;
}

void DownloadEngine::download_loop(std::stop_token stoken) noexcept {
    using namespace std::chrono_literals;

    // Start all segments
    for (auto& seg : segments_) {
        if (seg->state() == SegmentState::pending) {
            (void)seg->start();
        }
    }

    // Monitor progress
    while (!stoken.stop_requested() && state() == DownloadState::downloading) {
        update_progress();
        monitor_segments();

        if (config_.work_stealing) {
            attempt_work_stealing();
        }

        // Check completion (update_progress already writes completed/failed under lock)
        std::uint32_t completed = 0;
        std::uint32_t failed = 0;

        for (const auto& seg : segments_) {
            auto s = seg->state();
            if (s == SegmentState::completed) ++completed;
            if (s == SegmentState::failed) ++failed;
        }

        if (completed == segments_.size()) {
            state_.store(DownloadState::completed, std::memory_order_release);
            update_progress();  // Final callback after releasing lock
            break;
        }

        if (failed > 0 && completed + failed == segments_.size()) {
            state_.store(DownloadState::failed, std::memory_order_release);
            update_progress();  // Final callback after releasing lock
            break;
        }

        std::this_thread::sleep_for(BANDWIDTH_SAMPLE_INTERVAL);
    }
}

void DownloadEngine::monitor_segments() noexcept {
    using namespace std::chrono_literals;

    for (auto& seg : segments_) {
        if (seg->is_stalled(std::chrono::seconds{STALL_TIMEOUT_SEC})) {
            seg->state(SegmentState::stalled);
            // Auto-restart stalled segment
            (void)seg->resume();
        }
    }
}

void DownloadEngine::attempt_work_stealing() noexcept {
    if (segments_.size() < 2) return;

    for (auto& requester : segments_) {
        if (requester->state() != SegmentState::downloading) continue;

        // progress() now returns a copy, no lock needed
        const auto req_progress = requester->progress();

        // If this segment is much slower than others, try to steal
        // This is a simplified check - real implementation would be smarter
        if (req_progress.speed_bps < 100'000) { // < 100 KB/s
            auto steal_result = find_steal_target(segments_, requester->id(), 1'000'000);
            if (steal_result) {
                auto target_id = steal_result->first;
                auto bytes = steal_result->second;
                // Transfer bytes from target to requester
                for (auto& target : segments_) {
                    if (target->id() == target_id) {
                        target->steal_bytes(bytes);
                        requester->add_bytes(bytes); // Add stolen bytes to requester
                        break;
                    }
                }
            }
        }
    }
}

void DownloadEngine::update_progress() noexcept {
    // Gather segment data WITHOUT holding lock (atomics are lock-free)
    std::uint64_t total_downloaded = 0;
    double max_speed = 0.0;
    int active = 0, completed = 0, failed = 0;

    for (auto& seg : segments_) {
        total_downloaded += seg->downloaded();
        max_speed = std::max(max_speed, static_cast<double>(seg->progress().speed_bps));
        auto st = seg->state();
        if (st == SegmentState::downloading || st == SegmentState::connecting) ++active;
        else if (st == SegmentState::completed) ++completed;
        else if (st == SegmentState::failed) ++failed;
    }

    // Now hold lock ONLY to write aggregate progress struct and make snapshot
    DownloadProgress snap;
    DownloadCallback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        progress_.downloaded_bytes = total_downloaded;
        progress_.speed_bps = max_speed;
        progress_.active_segments = active;
        progress_.completed_segments = completed;
        progress_.failed_segments = failed;
        if (progress_.total_bytes > 0) {
            progress_.percent = static_cast<double>(total_downloaded) * 100.0
                                / static_cast<double>(progress_.total_bytes);
        }
        progress_.last_update = std::chrono::steady_clock::now();
        calculate_eta();
        snap = progress_;  // Copy under lock for callback
    }
    // Lock released here

    // Call callback with snapshot (copy under separate lock)
    {
        std::lock_guard<std::mutex> cb_lock(callback_mutex_);
        cb = callback_;
    }
    if (cb) {
        try { cb(snap); } catch (...) {}
    }
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

void DownloadEngine::stop_download() noexcept {
    // 1. Stop and join download loop thread FIRST
    // Prevents race where download_loop accesses segments while we cancel them
    if (download_thread_.joinable()) {
        download_thread_.request_stop();
        download_thread_.join();
    }

    // 2. NOW cancel all segments (stops curl transfers and joins their threads)
    for (auto& seg : segments_) {
        if (seg) seg->cancel();
    }

    // 3. Close file (all threads now done)
    if (file_writer_.is_open()) {
        (void)file_writer_.flush();
        file_writer_.close();
    }

    state_.store(DownloadState::paused, std::memory_order_release);
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
