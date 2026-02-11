// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/media/hls_parser.hpp>
#include <algorithm>
#include <regex>

namespace bolt::media {

namespace {

constexpr std::string_view TAG_EXTINF = "#EXTINF:";
constexpr std::string_view TAG_STREAM_INF = "#EXT-X-STREAM-INF:";
constexpr std::string_view TAG_TARGET_DURATION = "#EXT-X-TARGETDURATION:";
constexpr std::string_view TAG_MEDIA_SEQUENCE = "#EXT-X-MEDIA-SEQUENCE:";
constexpr std::string_view TAG_ENDLIST = "#EXT-X-ENDLIST";
constexpr std::string_view TAG_VERSION = "#EXT-X-VERSION:";
constexpr std::string_view TAG_BYTERANGE = "#EXT-X-BYTERANGE:";
constexpr std::string_view TAG_KEYS = "#EXT-X-KEY:";

} // namespace

bool HLSParser::is_hls_url(std::string_view url) noexcept {
    // Check for .m3u8 extension or HLS query parameters
    std::string lower_url;
    lower_url.reserve(url.size());
    for (char c : url) {
        lower_url += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return lower_url.find(".m3u8") != std::string::npos;
}

std::expected<HLSPlaylist, std::error_code>
HLSParser::parse(std::string_view content, std::string_view base_url) noexcept {
    HLSPlaylist playlist;
    playlist.type = HLSPlaylistType::vod;

    std::string_view::const_iterator it = content.begin();
    double current_duration = 0.0;
    std::uint64_t current_byte_offset = 0;
    std::uint64_t current_byte_length = 0;

    // Split by lines
    std::string line;
    while (it != content.end()) {
        if (*it == '\r' || *it == '\n') {
            ++it;
            continue;
        }

        auto end = std::find(it, content.end(), '\n');
        if (end == content.end()) {
            end = std::find(it, content.end(), '\r');
        }

        line = std::string(it, end);
        it = end;
        if (it != content.end()) ++it;

        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty() || line[0] == '#') {
            // Parse tags
            if (line.starts_with(TAG_TARGET_DURATION)) {
                auto val = line.substr(TAG_TARGET_DURATION.size());
                playlist.target_duration = std::stod(val);
            } else if (line.starts_with(TAG_STREAM_INF)) {
                // Variant playlist - parse bandwidth
                static const std::regex bandwidth_regex(R"(BANDWIDTH=(\d+))");
                std::cmatch match;
                if (std::regex_search(line.c_str(), match, bandwidth_regex)) {
                    HLSVariant variant;
                    variant.bandwidth = std::stoul(match[1].str());
                    playlist.variants.push_back(variant);
                }
            } else if (line.starts_with(TAG_EXTINF)) {
                auto val = line.substr(TAG_EXTINF.size());
                auto comma_pos = val.find(',');
                if (comma_pos != std::string_view::npos) {
                    val = val.substr(0, comma_pos);
                }
                current_duration = std::stod(std::string(val));
            } else if (line.starts_with(TAG_BYTERANGE)) {
                auto val = line.substr(TAG_BYTERANGE.size());
                auto at_pos = val.find('@');
                if (at_pos != std::string_view::npos) {
                    current_byte_length = std::stoull(std::string(val.substr(0, at_pos)));
                    current_byte_offset = std::stoull(std::string(val.substr(at_pos + 1)));
                }
            } else if (line == TAG_ENDLIST) {
                playlist.is_endless = false;
            }
        } else {
            // This is a segment URL
            HLSSegment segment;
            segment.url = resolve_url(base_url, line);
            segment.duration = current_duration;
            segment.byte_offset = current_byte_offset;
            segment.byte_length = current_byte_length;

            playlist.segments.push_back(segment);
            playlist.total_duration += static_cast<std::uint64_t>(current_duration * 1000);

            // Reset for next segment
            current_duration = 0.0;
            current_byte_offset = 0;
            current_byte_length = 0;
        }
    }

    // Determine if this is a live stream
    playlist.is_endless = (line.find(TAG_ENDLIST) == std::string::npos);

    return playlist;
}

std::string HLSParser::resolve_url(std::string_view base, std::string_view relative) noexcept {
    if (relative.starts_with("http://") || relative.starts_with("https://")) {
        return std::string(relative);
    }

    std::string base_str(base);

    // Remove filename from base URL
    auto last_slash = base_str.rfind('/');
    if (last_slash != std::string::npos) {
        base_str = base_str.substr(0, last_slash + 1);
    }

    if (relative.starts_with("/")) {
        // Absolute path
        auto scheme_end = base_str.find("://");
        if (scheme_end != std::string::npos) {
            auto host_start = base_str.find('/', scheme_end + 3);
            if (host_start != std::string::npos) {
                return base_str.substr(0, host_start) + std::string(relative);
            }
        }
    }

    // Relative path
    return base_str + std::string(relative);
}

} // namespace bolt::media
