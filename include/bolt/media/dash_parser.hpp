// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <string>
#include <vector>
#include <expected>
#include <cstdint>

namespace bolt::media {

// DASH (Dynamic Adaptive Streaming over HTTP) representation
struct DASHRepresentation {
    std::string id;
    std::uint32_t bandwidth{0};     // Bitrate in bps
    std::string mime_type;           // "video/mp4" or "audio/mp4"
    std::string codecs;
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t frame_rate{0};
    std::string initialization_url;
    std::vector<std::string> segment_urls;
    std::string template_url;        // Segment template with $Number$
};

// DASH adaptation set (group of representations)
struct DASHAdaptationSet {
    std::string id;
    std::string mime_type;
    std::string content_type;       // "video" or "audio"
    std::vector<DASHRepresentation> representations;
};

// DASH manifest (MPD)
struct DASHManifest {
    std::vector<DASHAdaptationSet> adaptation_sets;
    double min_buffer_time{0.0};
    std::uint64_t duration{0};     // Total duration in ms
    bool is_live{false};
    double minimum_update_period{0.0};
    double time_shift_buffer_depth{0.0};
};

// DASH MPD parser
class DASHParser {
public:
    // Parse MPD manifest content
    [[nodiscard]] static std::expected<DASHManifest, std::error_code>
    parse(std::string_view content, std::string_view base_url) noexcept;

    // Check if URL is a DASH manifest
    [[nodiscard]] static bool is_dash_url(std::string_view url) noexcept;

private:
    [[nodiscard]] static std::string resolve_url(
        std::string_view base,
        std::string_view relative) noexcept;
};

} // namespace bolt::media
