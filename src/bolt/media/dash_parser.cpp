// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/media/dash_parser.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace bolt::media {

bool DASHParser::is_dash_url(std::string_view url) noexcept {
    auto lower_url = std::string(url);
    std::transform(lower_url.begin(), lower_url.end(), lower_url.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return lower_url.find(".mpd") != std::string_view::npos;
}

std::expected<DASHManifest, std::error_code>
DASHParser::parse(std::string_view content, std::string_view base_url) noexcept {
    // Note: Full MPD parsing requires an XML parser
    // This is a simplified implementation
    DASHManifest manifest;

    // Check for MPD root element
    if (content.find("<MPD") == std::string_view::npos) {
        return std::unexpected(std::error_code());
    }

    // Parse basic manifest info
    auto type_pos = content.find("type=\"");
    if (type_pos != std::string_view::npos) {
        auto type_end = content.find("\"", type_pos + 6);
        auto type = content.substr(type_pos + 6, type_end - type_pos - 6);
        manifest.is_live = (type == "dynamic");
    }

    auto duration_pos = content.find("mediaPresentationDuration=\"");
    if (duration_pos != std::string_view::npos) {
        auto dur_end = content.find("\"", duration_pos + 25);
        // Parse PT format (e.g., "PT1H30M")
        // Simplified: just look for numbers
    }

    // Parse AdaptationSets and Representations
    // This requires proper XML parsing - placeholder for now

    return manifest;
}

std::string DASHParser::resolve_url(std::string_view base, std::string_view relative) noexcept {
    if (relative.starts_with("http://") || relative.starts_with("https://")) {
        return std::string(relative);
    }

    std::string base_str(base);
    auto last_slash = base_str.rfind('/');
    if (last_slash != std::string::npos) {
        base_str = base_str.substr(0, last_slash + 1);
    }

    return base_str + std::string(relative);
}

} // namespace bolt::media
