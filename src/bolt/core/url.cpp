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

    auto host_end = std::min(path_start, url_str.find('?', rest_start));
    auto at_pos = url_str.find('@', rest_start);
    auto colon_pos = url_str.rfind(':', host_end);

    std::size_t ipv6_end = 0;

    // Check for IPv6 address [::1]
    if (at_pos != std::string_view::npos && at_pos < host_end) {
        if (colon_pos > at_pos) {
            // Port is after @, like user@host:port
            url.host_ = std::string(url_str.substr(at_pos + 1, colon_pos - at_pos - 1));
            auto bracket_pos = url_str.find(']', at_pos);
            if (bracket_pos != std::string_view::npos) {
                url.host_ = std::string(url_str.substr(at_pos + 1, bracket_pos - at_pos));
            }
        }
    }

    // Extract host (before port or path)
    if (url.host_.empty()) {
        if (colon_pos != std::string_view::npos && colon_pos < host_end) {
            url.host_ = std::string(url_str.substr(rest_start, colon_pos - rest_start));
            auto port_end = std::min({url_str.find('/', colon_pos), url_str.find('?', colon_pos), host_end});
            url.port_ = std::string(url_str.substr(colon_pos + 1, port_end - colon_pos - 1));
        }

        if (url.host_.empty()) {
            auto bracket_end = url_str.find(']', rest_start);
            if (bracket_end != std::string_view::npos && bracket_end < host_end) {
                url.host_ = std::string(url_str.substr(bracket_end + 1, host_end - bracket_end - 1));
            }
        }
    }

    // Remaining is path
    if (path_start < url_str.length()) {
        url.path_ = std::string(url_str.substr(path_start));
    } else {
        url.path_ = "/";
    }

    // Parse query
    auto query_pos = url_str.find('?', path_start);
    if (query_pos != std::string_view::npos && query_pos < url_str.length()) {
        auto frag_pos = url_str.find('#', query_pos + 1);
        if (frag_pos != std::string_view::npos) {
            url.query_ = std::string(url_str.substr(query_pos + 1, frag_pos - query_pos - 1));
        } else {
            url.query_ = std::string(url_str.substr(query_pos + 1));
        }
    }

    // Parse fragment
    auto frag_pos = url_str.find('#', path_start);
    if (frag_pos != std::string_view::npos && frag_pos < url_str.length()) {
        url.fragment_ = std::string(url_str.substr(frag_pos + 1));
    }

    // Store the original string before returning
    url.set_str(std::string(url_str));
    return url;
}

std::string Url::full() const {
    std::string result;
    result.reserve(str_.size() + 10);

    result += scheme_;
    result += "://";
    result += host_;
    if (!port_.empty()) {
        result += ':';
        result += port_;
    }
    result += path_;
    if (!query_.empty()) {
        result += '?';
        result += query_;
    }
    if (!fragment_.empty()) {
        result += '#';
        result += fragment_;
    }

    return result;
}

std::string Url::base() const {
    std::string result;
    result.reserve(str_.size() + 10);

    result += scheme_;
    result += "://";
    result += host_;
    if (!port_.empty()) {
        result += ':';
        result += port_;
    }

    return result;
}

std::uint16_t Url::default_port() const noexcept {
    if (scheme_ == "http") return 80;
    if (scheme_ == "https") return 443;
    if (scheme_ == "ftp") return 21;
    return 0;
}

std::string Url::filename() const {
    std::string path = path_;  // Copy since path_ is now std::string

    // Remove query string if present
    auto query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path = path.substr(0, query_pos);
    }

    // Get filename from path
    auto last_slash = path.rfind('/');
    if (last_slash == std::string::npos) {
        last_slash = 0;
    }

    if (last_slash == 0) {
        return "index.html";
    }

    auto filename = path.substr(last_slash + 1);

    if (filename.empty() || filename == "/") {
        return "index.html";
    }

    return std::string(filename);
}

} // namespace bolt::core
