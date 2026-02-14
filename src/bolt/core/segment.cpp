// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/core/segment.hpp>
#include <bolt/core/config.hpp>
#include <curl/curl.h>
#include <algorithm>

namespace bolt::core {

namespace {

// libcurl write callback
std::size_t write_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) noexcept {
    auto* seg = static_cast<Segment*>(userdata);
    std::size_t bytes = size * nmemb;

    // Write data to file if file_writer is available
    auto* writer = seg->file_writer();
    if (writer) {
        std::uint64_t file_offset = seg->file_offset() + seg->write_offset();
        auto ec = writer->write(file_offset, ptr, bytes);
        if (ec) {
            // Write failed - return 0 to abort the download
            return 0;
        }
    } else {
        return 0;
    }

    // Update progress using atomics (no mutex - no deadlock possible)
    seg->add_downloaded(bytes);
    return bytes;
}

// libcurl progress callback - checks stop_requested and aborts if set
int progress_callback(void* userdata, curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow) noexcept {
    (void)dltotal; (void)dlnow; (void)ultotal; (void)ulnow;
    auto* seg = static_cast<Segment*>(userdata);
    // Return 1 to abort the transfer
    return seg->stop_requested() ? 1 : 0;
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
    , curl_handle_(other.curl_handle_)
    , atomic_downloaded_(other.atomic_downloaded_.load(std::memory_order_relaxed))
    , atomic_write_offset_(other.atomic_write_offset_.load(std::memory_order_relaxed)) {
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

        atomic_downloaded_.store(other.atomic_downloaded_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        atomic_write_offset_.store(other.atomic_write_offset_.load(std::memory_order_relaxed), std::memory_order_relaxed);

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

    try {
        progress_.start_time = std::chrono::steady_clock::now();
        progress_.last_update = progress_.start_time;
    } catch (...) {
        return make_error_code(DownloadErrc::network_error);
    }

    // Spawn download thread - download happens asynchronously
    segment_thread_ = std::jthread([this](std::stop_token stoken) {
        try {
            // Initialize libcurl handle
            auto* curl = curl_easy_init();
            if (!curl) {
                state(SegmentState::failed);
                error_ = make_error_code(DownloadErrc::network_error);
                return;
            }

            curl_handle_ = curl;

            // Set URL
            std::string url_str = url_.full();
            curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());

            // Set range header - resume from where we left off
            std::uint64_t downloaded = atomic_downloaded_.load(std::memory_order_relaxed);
            std::uint64_t start_byte = offset_ + downloaded;
            std::uint64_t end_byte = offset_ + size_ - 1;

            if (start_byte > end_byte) {
                // Already fully downloaded
                state(SegmentState::completed);
                curl_easy_cleanup(curl);
                curl_handle_ = nullptr;
                return;
            }

            std::string range = std::to_string(start_byte) + "-" + std::to_string(end_byte);
            curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());

            // Set callbacks
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, reinterpret_cast<void*>(write_callback));
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

            // Set progress callback to allow interruption
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, reinterpret_cast<void*>(progress_callback));
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);  // Enable progress callback

            // Set header callback
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, reinterpret_cast<void*>(header_callback));
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

            // Buffer sizes for better throughput
            curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, static_cast<long>(WRITE_BUFFER_SIZE));

            // TCP optimizations - disable Nagle's algorithm for lower latency
            curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);

            // Share DNS cache and SSL sessions between segments
            if (curl_share_handle_) {
                curl_easy_setopt(curl, CURLOPT_SHARE, curl_share_handle_);
            }

            // Pre-resolved DNS - skip DNS lookup if we have the IP
            if (!resolved_ip_.empty()) {
                struct curl_slist* dns = nullptr;
                std::string resolve_entry = url_.host() + ":" + url_.port() + ":" + resolved_ip_;
                dns = curl_slist_append(dns, resolve_entry.c_str());
                curl_easy_setopt(curl, CURLOPT_RESOLVE, dns);
            }

            state(SegmentState::downloading);

            auto result = curl_easy_perform(curl);

            // Get HTTP code BEFORE cleanup (only valid if curl succeeded)
            long http_code = 0;
            if (result == CURLE_OK) {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            }

            // ALWAYS clean up curl handle immediately after perform
            curl_easy_cleanup(curl);
            curl_handle_ = nullptr;

            // Handle cancellation (not an error)
            if (result == CURLE_ABORTED_BY_CALLBACK) {
                return;  // Cancelled via progress callback - not a failure
            }

            if (result != CURLE_OK) {
                // Retry transient network errors up to 3 times
                if (result == CURLE_RECV_ERROR || result == CURLE_COULDNT_CONNECT ||
                    result == CURLE_OPERATION_TIMEDOUT || result == CURLE_SSL_CONNECT_ERROR) {
                    int retries = 0;
                    constexpr int MAX_RETRIES = 3;
                    while (retries < MAX_RETRIES && !stop_requested_.load(std::memory_order_relaxed)) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));  // Fast retry
                        ++retries;
                        fprintf(stderr, "Segment %u: retry %d/%d after error %d\n", id_, retries, MAX_RETRIES, result);

                        // Re-create curl handle and retry
                        curl = curl_easy_init();
                        if (!curl) continue;
                        curl_handle_ = curl;

                        // Re-configure curl (same as above)
                        curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
                        std::uint64_t downloaded = atomic_downloaded_.load(std::memory_order_relaxed);
                        std::uint64_t start_byte = offset_ + downloaded;
                        std::uint64_t end_byte = offset_ + size_ - 1;
                        if (start_byte > end_byte) {
                            curl_easy_cleanup(curl);
                            curl_handle_ = nullptr;
                            state(SegmentState::completed);
                            return;
                        }
                        std::string range = std::to_string(start_byte) + "-" + std::to_string(end_byte);
                        curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
                        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, reinterpret_cast<void*>(write_callback));
                        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
                        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, reinterpret_cast<void*>(progress_callback));
                        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
                        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, reinterpret_cast<void*>(header_callback));
                        curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
                        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(CONNECTION_TIMEOUT_SEC));
                        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, static_cast<long>(STALL_TIMEOUT_SEC));
                        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
                        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
                        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
                        if constexpr (FOLLOW_REDIRECTS) {
                            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(MAX_REDIRECTS));
                        }
                        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
                        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, static_cast<long>(WRITE_BUFFER_SIZE));
                        curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);

                        result = curl_easy_perform(curl);
                        curl_easy_cleanup(curl);
                        curl_handle_ = nullptr;

                        if (result == CURLE_OK) {
                            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                            break;
                        }
                        if (result == CURLE_ABORTED_BY_CALLBACK) {
                            return;
                        }
                    }
                }

                if (result != CURLE_OK) {
                    fprintf(stderr, "Segment %u: curl error %d: %s\n", id_, result, curl_easy_strerror(result));
                    state(SegmentState::failed);
                    error_ = make_error_code(DownloadErrc::network_error);
                    return;
                }
            }

            if (http_code >= 400) {
                state(SegmentState::failed);
                if (http_code == 416) {
                    error_ = make_error_code(DownloadErrc::invalid_range);
                } else if (http_code == 404) {
                    error_ = make_error_code(DownloadErrc::not_found);
                } else {
                    error_ = make_error_code(DownloadErrc::server_error);
                }
                return;
            }

            state(SegmentState::completed);
        } catch (...) {
            state(SegmentState::failed);
            error_ = make_error_code(DownloadErrc::network_error);
        }
    });

    return {};
}

void Segment::pause() noexcept {
    // TODO: Implement pause with CURL_PAUSE
}

std::error_code Segment::resume() noexcept {
    if (state() == SegmentState::stalled) {
        // Stop the stalled transfer first
        stop_requested_.store(true, std::memory_order_release);

        // Wait for the old thread to finish
        if (segment_thread_.joinable()) {
            segment_thread_.request_stop();
            segment_thread_.join();
        }

        // Clean up curl handle if still present
        if (curl_handle_) {
            curl_easy_cleanup(static_cast<CURL*>(curl_handle_));
            curl_handle_ = nullptr;
        }

        // Reset stop flag for new transfer
        stop_requested_.store(false, std::memory_order_release);

        // Reset progress tracking for the restart
        progress_.last_update = std::chrono::steady_clock::now();
        progress_.start_time = progress_.last_update;

        // Set state to pending before calling start() - start() expects pending state
        state(SegmentState::pending);

        // Restart from where we left off
        return start();
    }
    return {};
}

void Segment::cancel() noexcept {
    // Signal curl to abort via progress callback
    stop_requested_.store(true, std::memory_order_release);

    // Request thread stop and wait for completion
    if (segment_thread_.joinable()) {
        segment_thread_.request_stop();
        segment_thread_.join();
    }

    // Clean up curl handle if still owned
    if (curl_handle_) {
        curl_easy_cleanup(static_cast<CURL*>(curl_handle_));
        curl_handle_ = nullptr;
    }

    // Set cancelled state AFTER thread is done and cleanup is complete
    // This allows segments to naturally complete (failed or success) before
    // being marked as cancelled, which is important for restart scenarios.
    state(SegmentState::cancelled);
}

bool Segment::is_stalled(std::chrono::seconds timeout) const noexcept {
    if (state() != SegmentState::downloading) return false;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - progress_.last_update);
    return elapsed >= timeout;
}

std::uint64_t Segment::can_steal(std::uint64_t min_steal) const noexcept {
    if (state() != SegmentState::downloading) return 0;

    std::uint64_t remaining = size_ - atomic_downloaded_.load(std::memory_order_relaxed);
    if (remaining <= min_steal * 2) return 0; // Keep at least min_steal

    return (remaining / 2) & ~0xFFFULL; // Align to 4KB boundary, steal half
}

void Segment::steal_bytes(std::uint64_t bytes) noexcept {
    size_ -= bytes;
    // Note: file_offset stays the same, we just reduce HTTP range
}

void Segment::add_bytes(std::uint64_t bytes) noexcept {
    size_ += bytes;
    // Note: file_offset stays the same, we just increase HTTP range
}

void Segment::reduce_range(std::uint64_t new_end) noexcept {
    // Reduce segment size to end at new_end
    // This is called during dynamic segmentation when splitting a segment
    if (new_end > offset_) {
        std::uint64_t new_size = new_end - offset_;
        if (new_size < size_) {
            size_ = new_size;
            progress_.total_bytes = new_size;
        }
    }
}

std::uint64_t Segment::remaining() const noexcept {
    auto downloaded = atomic_downloaded_.load(std::memory_order_relaxed);
    if (downloaded >= size_) return 0;
    return size_ - downloaded;
}

double Segment::percent() const noexcept {
    if (size_ == 0) return 100.0;
    auto downloaded = atomic_downloaded_.load(std::memory_order_relaxed);
    return static_cast<double>(downloaded) * 100.0 / static_cast<double>(size_);
}

void Segment::add_downloaded(std::uint64_t bytes) noexcept {
    auto now = std::chrono::steady_clock::now();

    // Update atomic counters (no mutex - no deadlock possible)
    atomic_downloaded_.fetch_add(bytes, std::memory_order_relaxed);
    atomic_write_offset_.fetch_add(bytes, std::memory_order_relaxed);

    // Accumulate bytes for instantaneous speed calculation
    atomic_speed_bytes_.fetch_add(bytes, std::memory_order_relaxed);

    // Calculate time delta from last update for instantaneous speed
    auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - progress_.last_update).count();
    auto total_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - progress_.start_time).count();

    if (total_elapsed_ms > 0) {
        // Average speed: total bytes / total time
        auto downloaded = atomic_downloaded_.load(std::memory_order_relaxed);
        progress_.average_speed_bps = static_cast<std::uint64_t>(
            (static_cast<double>(downloaded) * 1000.0) / total_elapsed_ms);
    }

    // Instantaneous speed: accumulated bytes since last update / time since last update
    // Use a small minimum window (100ms) to avoid spikes
    if (delta_ms >= 100) {
        auto accumulated_bytes = atomic_speed_bytes_.exchange(0, std::memory_order_relaxed);
        progress_.speed_bps = static_cast<std::uint64_t>(
            (static_cast<double>(accumulated_bytes) * 1000.0) / delta_ms);
        progress_.last_update = now;
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
