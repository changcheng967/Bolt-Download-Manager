// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/core/segment.hpp>
#include <bolt/core/config.hpp>
#include <curl/curl.h>
#include <algorithm>

namespace bolt::core {

namespace {

// libcurl write callback
std::size_t write_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* seg = static_cast<Segment*>(userdata);
    std::size_t bytes = size * nmemb;
    seg->add_downloaded(bytes);
    return bytes;
}

// libcurl header callback (for content-range validation)
std::size_t header_callback(char* buffer, std::size_t size, std::size_t nitems, void* userdata) {
    return size * nitems;
}

} // namespace

//=============================================================================
// Segment
//=============================================================================

Segment::Segment(std::uint32_t id, Url url,
                 std::uint64_t offset, std::uint64_t size,
                 std::uint64_t file_offset) noexcept
    : id_(id)
    , url_(std::move(url))
    , offset_(offset)
    , size_(size)
    , file_offset_(file_offset)
    , progress_{0, size, 0, 0, std::chrono::steady_clock::now(), std::chrono::steady_clock::now()} {}

Segment::Segment(Segment&& other) noexcept
    : id_(other.id_)
    , url_(std::move(other.url_))
    , offset_(other.offset_)
    , size_(other.size_)
    , file_offset_(other.file_offset_)
    , state_(other.state_.load())
    , progress_(other.progress_)
    , error_(other.error_)
    , curl_handle_(other.curl_handle_) {
    other.curl_handle_ = nullptr;
    other.state_.store(SegmentState::pending, std::memory_order_release);
}

Segment& Segment::operator=(Segment&& other) noexcept {
    if (this != &other) {
        if (curl_handle_) {
            curl_easy_cleanup(static_cast<CURL*>(curl_handle_));
        }

        id_ = other.id_;
        url_ = std::move(other.url_);
        offset_ = other.offset_;
        size_ = other.size_;
        file_offset_ = other.file_offset_;
        state_.store(other.state_.load());
        progress_ = other.progress_;
        error_ = other.error_;
        curl_handle_ = other.curl_handle_;

        other.curl_handle_ = nullptr;
        other.state_.store(SegmentState::pending, std::memory_order_release);
    }
    return *this;
}

std::error_code Segment::start() noexcept {
    auto expected = SegmentState::pending;
    if (!state_.compare_exchange_strong(expected, SegmentState::connecting,
                                         std::memory_order_acq_rel)) {
        return make_error_code(DownloadErrc::network_error);
    }

    progress_.start_time = std::chrono::steady_clock::now();
    progress_.last_update = progress_.start_time;

    // Initialize libcurl handle
    auto* curl = curl_easy_init();
    if (!curl) {
        state(SegmentState::failed);
        return make_error_code(DownloadErrc::network_error);
    }

    curl_handle_ = curl;

    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url_.full().c_str());

    // Set range header
    std::string range = std::to_string(offset_) + "-" + std::to_string(offset_ + size_ - 1);
    curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());

    // Set callbacks
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

    // Timeouts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(CONNECTION_TIMEOUT_SEC));
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, static_cast<long>(STALL_TIMEOUT_SEC));
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);

    // SSL options
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    // Follow redirects
    if constexpr (FOLLOW_REDIRECTS) {
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(MAX_REDIRECTS));
    }

    // HTTP/2
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);

    state(SegmentState::downloading);

    auto result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        state(SegmentState::failed);
        error_ = make_error_code(DownloadErrc::network_error);
        return error_;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code >= 400) {
        state(SegmentState::failed);
        if (http_code == 416) {
            error_ = make_error_code(DownloadErrc::invalid_range);
        } else if (http_code == 404) {
            error_ = make_error_code(DownloadErrc::not_found);
        } else {
            error_ = make_error_code(DownloadErrc::server_error);
        }
        return error_;
    }

    state(SegmentState::completed);
    return {};
}

void Segment::pause() noexcept {
    // TODO: Implement pause with CURL_PAUSE
}

std::error_code Segment::resume() noexcept {
    if (state() == SegmentState::stalled) {
        return start(); // Restart stalled segment
    }
    return {};
}

void Segment::cancel() noexcept {
    state(SegmentState::cancelled);
    if (curl_handle_) {
        curl_easy_cleanup(static_cast<CURL*>(curl_handle_));
        curl_handle_ = nullptr;
    }
}

bool Segment::is_stalled(std::chrono::seconds timeout) const noexcept {
    if (state() != SegmentState::downloading) return false;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - progress_.last_update);
    return elapsed >= timeout;
}

std::uint64_t Segment::can_steal(std::uint64_t min_steal) const noexcept {
    if (state() != SegmentState::downloading) return 0;

    std::uint64_t remaining = size_ - progress_.downloaded_bytes;
    if (remaining <= min_steal * 2) return 0; // Keep at least min_steal

    return (remaining / 2) & ~0xFFFUL; // Align to 4KB boundary, steal half
}

void Segment::steal_bytes(std::uint64_t bytes) noexcept {
    size_ -= bytes;
    // Note: file_offset stays the same, we just reduce HTTP range
}

void Segment::add_bytes(std::uint64_t bytes) noexcept {
    size_ += bytes;
    // Note: file_offset stays the same, we just increase HTTP range
}

std::uint64_t Segment::remaining() const noexcept {
    if (progress_.downloaded_bytes >= size_) return 0;
    return size_ - progress_.downloaded_bytes;
}

double Segment::percent() const noexcept {
    if (size_ == 0) return 100.0;
    return static_cast<double>(progress_.downloaded_bytes) * 100.0 / static_cast<double>(size_);
}

void Segment::add_downloaded(std::uint64_t bytes) noexcept {
    auto lock = lock();
    progress_.downloaded_bytes += bytes;
    progress_.last_update = std::chrono::steady_clock::now();

    // Calculate instantaneous speed
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        progress_.last_update - progress_.start_time);
    if (elapsed.count() > 0) {
        progress_.speed_bps = static_cast<std::uint64_t>(
            (static_cast<double>(bytes) * 1000.0) / elapsed.count());
        progress_.average_speed_bps = static_cast<std::uint64_t>(
            (static_cast<double>(progress_.downloaded_bytes) * 1000.0) / elapsed.count());
    }
}

//=============================================================================
// Work stealing
//=============================================================================

std::expected<std::pair<std::uint32_t, std::uint64_t>, std::error_code>
find_steal_target(const std::vector<std::unique_ptr<Segment>>& segments,
                  std::uint32_t requester_id,
                  std::uint64_t min_bytes) noexcept {
    std::uint32_t best_target = 0;
    std::uint64_t most_removable = 0;

    for (const auto& seg : segments) {
        if (!seg) continue;
        if (seg->id() == requester_id) continue;
        if (seg->state() != SegmentState::downloading) continue;

        std::uint64_t removable = seg->can_steal(min_bytes);
        if (removable > most_removable) {
            most_removable = removable;
            best_target = seg->id();
        }
    }

    if (most_removable == 0) {
        return std::unexpected(make_error_code(DownloadErrc::invalid_range));
    }

    return {{best_target, most_removable}};
}

} // namespace bolt::core
