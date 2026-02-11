// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/media/hls_parser.hpp>
#include <bolt/media/dash_parser.hpp>
#include <bolt/core/download_engine.hpp>
#include <string>
#include <vector>
#include <expected>
#include <functional>
#include <memory>

namespace bolt::media {

// Media download progress
struct MediaProgress {
    std::uint64_t downloaded_bytes{0};
    std::uint64_t total_bytes{0};
    std::uint32_t segments_downloaded{0};
    std::uint32_t total_segments{0};
    std::uint64_t speed_bps{0};
    double percent{0.0};
};

// Callback for media download progress
using MediaProgressCallback = std::function<void(const MediaProgress&)>;

// Media downloader for HLS and DASH streams
class MediaDownloader {
public:
    MediaDownloader();
    ~MediaDownloader();

    // Non-copyable, movable
    MediaDownloader(const MediaDownloader&) = delete;
    MediaDownloader& operator=(const MediaDownloader&) = delete;

    // Detect and parse media manifest
    [[nodiscard]] std::expected<bool, std::error_code>
    detect_manifest(std::string_view url) noexcept;

    // Download HLS stream
    [[nodiscard]] std::error_code download_hls(
        const std::string& url,
        const std::string& output_path) noexcept;

    // Download DASH stream
    [[nodiscard]] std::error_code download_dash(
        const std::string& url,
        const std::string& output_path) noexcept;

    // Set progress callback
    void callback(MediaProgressCallback cb) noexcept { callback_ = std::move(cb); }

    // Cancel download
    void cancel() noexcept;

    // Get current playlist/manifest
    [[nodiscard]] const HLSPlaylist& hls_playlist() const noexcept { return hls_playlist_; }
    [[nodiscard]] const DASHManifest& dash_manifest() const noexcept { return dash_manifest_; }

private:
    // Download and parse manifest
    [[nodiscard]] std::error_code fetch_hls_playlist(const std::string& url) noexcept;
    [[nodiscard]] std::error_code fetch_dash_manifest(const std::string& url) noexcept;

    // Download HLS segments
    [[nodiscard]] std::error_code download_hls_segments(
        const std::string& output_path) noexcept;

    // Download HLS segment
    [[nodiscard]] std::error_code download_segment(
        const HLSSegment& segment,
        std::uint64_t offset,
        const std::string& temp_path) noexcept;

    // Update progress
    void update_progress() noexcept;

    HLSPlaylist hls_playlist_;
    DASHManifest dash_manifest_;
    MediaProgressCallback callback_;
    MediaProgress progress_;

    std::atomic<bool> cancelled_{false};
    std::unique_ptr<DownloadEngine> http_client_;
};

// Factory to detect and create appropriate downloader
class MediaGrabber {
public:
    // Detect if URL is a media stream
    [[nodiscard]] static bool is_media_url(std::string_view url) noexcept;

    // Get manifest URL from page (for embedded video)
    [[nodiscard]] static std::vector<std::string>
    extract_media_urls(std::string_view page_content) noexcept;

    // Create appropriate downloader
    [[nodiscard]] static std::expected<std::unique_ptr<MediaDownloader>, std::error_code>
    create(std::string_view url) noexcept;
};

} // namespace bolt::media
