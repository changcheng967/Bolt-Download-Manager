// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <string>
#include <vector>
#include <expected>
#include <cstdint>

namespace bolt::media {

// HLS (HTTP Live Streaming) segment
struct HLSSegment {
    std::string url;
    double duration{0.0};          // Segment duration in seconds
    std::uint64_t byte_offset{0};   // For byterange playlists
    std::uint64_t byte_length{0};
};

// HLS variant (for adaptive bitrate)
struct HLSVariant {
    std::uint32_t bandwidth{0};     // Bitrate in bps
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::string codecs;
    std::string url;
};

// Playlist types
enum class HLSPlaylistType {
    unknown,
    vod,          // Video on demand
    event,        // Event
    live          // Live stream
};

// Parsed HLS playlist
struct HLSPlaylist {
    HLSPlaylistType type{HLSPlaylistType::unknown};
    std::vector<HLSSegment> segments;
    std::vector<HLSVariant> variants;
    double target_duration{0.0};
    std::uint64_t total_duration{0};
    bool is_endless{false};         // Live stream
    std::string encryption_method;
    std::string encryption_key_uri;
};

// HLS M3U8 parser
class HLSParser {
public:
    // Parse M3U8 playlist content
    [[nodiscard]] static std::expected<HLSPlaylist, std::error_code>
    parse(std::string_view content, std::string_view base_url) noexcept;

    // Check if URL is an HLS playlist
    [[nodiscard]] static bool is_hls_url(std::string_view url) noexcept;

private:
    [[nodiscard]] static std::string resolve_url(
        std::string_view base,
        std::string_view relative) noexcept;
};

} // namespace bolt::media
