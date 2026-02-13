// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/core/url.hpp>
#include <algorithm>
#include <cctype>

namespace bolt::core {

std::expected<Url, std::error_code> Url::parse(std::string_view url_str) noexcept {
    Url url;  // Don't store input string yet - will set after parsing

    // Parse scheme
    auto scheme_end = url_str.find("://");
    if (scheme_end == std::string_view::npos) {
        return std::unexpected(make_error_code(DownloadErrc::invalid_url));
    }

    // Convert scheme to lowercase and store
    std::string lower_scheme;
    lower_scheme.reserve(scheme_end);
    for (std::size_t i = 0; i < scheme_end; ++i) {
        lower_scheme += static_cast<char>(std::tolower(static_cast<unsigned char>(url_str[i])));
    }
    url.scheme_ = std::move(lower_scheme);

    auto rest_start = scheme_end + 3; // Skip "://"

    // Parse host and optional port
    auto path_start = url_str.find('/', rest_start);
    if (path_start == std::string_view::npos) {
        path_start = url_str.length();
    }

    auto query_start = url_str.find('?', rest_start);
    if (query_start == std::string_view::npos) {
        query_start = url_str.length();
    }

    auto fragment_start = url_str.find('#', rest_start);
    if (fragment_start == std::string_view::npos) {
        fragment_start = url_str.length();
    }

    // host_end is at the first of: /, ?, #, or end
    auto host_end = std::min({path_start, query_start, fragment_start, url_str.length()});

    // Find the colon that separates host from port (in authority part)
    auto colon_pos = url_str.find(':', rest_start);

    // Check for userinfo @ notation (user:pass@host:port)
    auto at_pos = url_str.find('@', rest_start);

    // Helper: Check for IPv6 address [2001:db8::1]
    auto bracket_start = url_str.find('[', rest_start);
    std::size_t authority_start = rest_start;

    if (at_pos != std::string_view::npos && at_pos < host_end) {
        // Skip userinfo portion
        authority_start = at_pos + 1;
    }

    // Handle IPv6 address in brackets [::1]:port
    if (bracket_start != std::string_view::npos && bracket_start < host_end) {
        auto bracket_end = url_str.find(']', bracket_start);
        if (bracket_end != std::string_view::npos && bracket_end < host_end) {
            url.host_ = std::string(url_str.substr(bracket_start, bracket_end - bracket_start + 1));
            // Port is after the closing bracket
            auto ipv6_colon = url_str.find(':', bracket_end);
            if (ipv6_colon != std::string_view::npos && ipv6_colon < host_end) {
                url.port_ = std::string(url_str.substr(ipv6_colon + 1, host_end - ipv6_colon - 1));
            }
        } else {
            // Malformed IPv6, treat as regular host from bracket_start
            url.host_ = std::string(url_str.substr(authority_start, host_end - authority_start));
        }
    } else {
        // Regular (non-IPv6) host extraction
        if (colon_pos != std::string_view::npos && colon_pos > authority_start && colon_pos < host_end) {
            // Port present: extract host before colon
            url.host_ = std::string(url_str.substr(authority_start, colon_pos - authority_start));
            url.port_ = std::string(url_str.substr(colon_pos + 1, host_end - colon_pos - 1));
        } else {
            // No port: extract entire authority portion as host
            url.host_ = std::string(url_str.substr(authority_start, host_end - authority_start));
        }
    }

    // Extract path (if present)
    if (path_start < url_str.length()) {
        auto path_end = std::min(query_start, fragment_start);
        url.path_ = std::string(url_str.substr(path_start, path_end - path_start));
    } else {
        url.path_ = "/";
    }

    // Extract query (if present)
    if (query_start < url_str.length() && query_start < fragment_start) {
        auto query_end = fragment_start;
        url.query_ = std::string(url_str.substr(query_start + 1, query_end - query_start - 1));
    }

    // Extract fragment (if present)
    if (fragment_start < url_str.length()) {
        url.fragment_ = std::string(url_str.substr(fragment_start + 1));
    }

    // Validate that we got a host
    if (url.host_.empty()) {
        return std::unexpected(make_error_code(DownloadErrc::invalid_url));
    }

    // Store the full URL string
    url.set_str(std::string(url_str));

    return url;
}

std::string Url::full() const {
    std::string result = scheme_;
    result += "://";
    result += host_;
    if (!port_.empty()) {
        result += ":";
        result += port_;
    }
    if (!path_.empty()) {
        result += path_;
    }
    if (!query_.empty()) {
        result += "?";
        result += query_;
    }
    if (!fragment_.empty()) {
        result += "#";
        result += fragment_;
    }
    return result;
}

std::string Url::base() const {
    std::string result = scheme_;
    result += "://";
    result += host_;
    if (!port_.empty()) {
        result += ":";
        result += port_;
    }
    return result;
}

std::uint16_t Url::default_port() const noexcept {
    if (scheme_ == "http") return 80;
    if (scheme_ == "https") return 443;
    if (scheme_ == "ftp") return 21;
    if (scheme_ == "sftp") return 22;
    return 0;
}

std::string Url::filename() const {
    // Find the last '/' in path and extract filename
    auto last_slash = path_.rfind('/');
    if (last_slash == std::string::npos) {
        return path_.empty() ? "index.html" : path_;
    }
    auto filename = path_.substr(last_slash + 1);
    // For directory URLs (path ends with /), default to index.html
    if (filename.empty()) {
        return "index.html";
    }
    return filename;
}

} // namespace bolt::core
