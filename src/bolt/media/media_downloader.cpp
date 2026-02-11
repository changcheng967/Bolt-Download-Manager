// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/media/media_downloader.hpp>
#include <algorithm>
#include <regex>
#include <thread>

namespace bolt::media {

//=============================================================================
// MediaDownloader
//=============================================================================

MediaDownloader::MediaDownloader() = default;

MediaDownloader::~MediaDownloader() {
    cancel();
}

std::expected<bool, std::error_code>
MediaDownloader::detect_manifest(std::string_view url) noexcept {
    if (HLSParser::is_hls_url(url)) {
        return true;
    }
    if (DASHParser::is_dash_url(url)) {
        return true;
    }
    return false;
}

std::error_code MediaDownloader::fetch_hls_playlist(const std::string& url) noexcept {
    HttpSession session;
    auto response = session.head(url);

    if (!response) {
        return response.error();
    }

    // TODO: Get actual content and parse
    hls_playlist_ = HLSPlaylist{};
    return {};
}

std::error_code MediaDownloader::download_hls(
    const std::string& url,
    const std::string& output_path) noexcept {

    auto err = fetch_hls_playlist(url);
    if (err) {
        return err;
    }

    if (hls_playlist_.segments.empty()) {
        return make_error_code(core::DownloadErrc::invalid_url);
    }

    progress_.total_segments = static_cast<std::uint32_t>(hls_playlist_.segments.size());

    return download_hls_segments(output_path);
}

std::error_code MediaDownloader::download_hls_segments(
    const std::string& output_path) noexcept {

    std::string temp_path = output_path + ".temp";

    // Calculate total size
    for (const auto& seg : hls_playlist_.segments) {
        progress_.total_bytes += seg.byte_length > 0 ? seg.byte_length : 1'000'000;
    }

    std::uint64_t current_offset = 0;

    for (std::size_t i = 0; i < hls_playlist_.segments.size(); ++i) {
        if (cancelled_) {
            return make_error_code(core::DownloadErrc::cancelled);
        }

        const auto& seg = hls_playlist_.segments[i];
        auto err = download_segment(seg, current_offset, temp_path);

        if (err) {
            return err;
        }

        progress_.segments_downloaded++;
        current_offset += seg.byte_length > 0 ? seg.byte_length : 1'000'000;
        update_progress();
    }

    // Rename temp file to final
    // TODO: Implement rename
    return {};
}

std::error_code MediaDownloader::download_segment(
    const HLSSegment& segment,
    std::uint64_t offset,
    const std::string& temp_path) noexcept {

    // TODO: Use download engine to download segment
    return {};
}

void MediaDownloader::update_progress() noexcept {
    if (progress_.total_bytes > 0) {
        progress_.percent = static_cast<double>(progress_.downloaded_bytes) * 100.0
                         / static_cast<double>(progress_.total_bytes);
    }

    if (callback_) {
        callback_(progress_);
    }
}

void MediaDownloader::cancel() noexcept {
    cancelled_ = true;
}

//=============================================================================
// MediaGrabber
//=============================================================================

bool MediaGrabber::is_media_url(std::string_view url) noexcept {
    return HLSParser::is_hls_url(url) || DASHParser::is_dash_url(url);
}

std::vector<std::string>
MediaGrabber::extract_media_urls(std::string_view page_content) noexcept {
    std::vector<std::string> urls;

    // Regex patterns for common media URLs
    static const std::vector<std::regex> patterns = {
        std::regex(R"(https?://[^\s"'<>]+\.m3u8[^\s"'<>]*)"),
        std::regex(R"(https?://[^\s"'<>]+\.mpd[^\s"'<>]*)"),
        std::regex(R"(["']https?://[^"'<>]*\.(mp4|webm|ogg)[^"'<>]*["'])"),
    };

    for (const auto& pattern : patterns) {
        std::cregex_iterator it(page_content.begin(), page_content.end(), pattern);
        std::cregex_iterator end;

        while (it != end) {
            std::string url = (*it)[1].str();

            // Clean up quotes
            if (!url.empty() && (url.front() == '\'' || url.front() == '"')) {
                url = url.substr(1);
            }
            if (!url.empty() && (url.back() == '\'' || url.back() == '"')) {
                url.pop_back();
            }

            urls.push_back(url);
            ++it;
        }
    }

    return urls;
}

std::expected<std::unique_ptr<MediaDownloader>, std::error_code>
MediaGrabber::create(std::string_view url) noexcept {
    auto downloader = std::make_unique<MediaDownloader>();

    auto detected = downloader->detect_manifest(url);
    if (!detected) {
        return std::unexpected(detected.error());
    }

    if (!*detected) {
        return std::unexpected(make_error_code(core::DownloadErrc::invalid_url));
    }

    return downloader;
}

} // namespace bolt::media
