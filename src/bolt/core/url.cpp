// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/core/url.hpp>
#include <algorithm>
#include <cctype>

namespace bolt::core {

std::expected<Url, std::error_code> Url::parse(std::string_view url_str) noexcept {
    Url url;
    url.str_ = url_str;

    std::size_t pos = 0;

    // Parse scheme
    auto scheme_end = url_str.find("://");
    if (scheme_end == std::string_view::npos) {
        return std::unexpected(make_error_code(DownloadErrc::invalid_url));
    }

    url.scheme_ = url_str.substr(0, scheme_end);
    std::transform(url.scheme_.begin(), url.scheme_.end(),
                   url.scheme_.begin(), [](unsigned char c) { return std::tolower(c); });

    pos = scheme_end + 3; // Skip "://"

    // Parse host and optional port
    auto path_start = url_str.find('/', pos);
    auto query_start = url_str.find('?', pos);
    auto frag_start = url_str.find('#', pos);

    std::size_t host_end = path_start;
    if (query_start != std::string_view::npos && (host_end == std::string_view::npos || query_start < host_end)) {
        host_end = query_start;
    }
    if (frag_start != std::string_view::npos && (host_end == std::string_view::npos || frag_start < host_end)) {
        host_end = frag_start;
    }
    if (host_end == std::string_view::npos) {
        host_end = url_str.length();
    }

    auto colon_pos = url_str.rfind(':', host_end);
    if (colon_pos != std::string_view::npos && colon_pos > pos) {
        // Check if it's an IPv6 address
        if (url_str[pos] == '[') {
            auto closing_bracket = url_str.find(']', pos);
            if (closing_bracket != std::string_view::npos && closing_bracket < colon_pos) {
                url.host_ = url_str.substr(pos, closing_bracket - pos + 1);
                url.port_ = url_str.substr(colon_pos + 1, host_end - colon_pos - 1);
            }
        } else {
            url.host_ = url_str.substr(pos, colon_pos - pos);
            url.port_ = url_str.substr(colon_pos + 1, host_end - colon_pos - 1);
        }
    } else {
        url.host_ = url_str.substr(pos, host_end - pos);
    }

    pos = host_end;

    // Parse path
    if (pos < url_str.length() && url_str[pos] == '/') {
        std::size_t path_end = url_str.length();
        if (query_start != std::string_view::npos && query_start < path_end) {
            path_end = query_start;
        }
        if (frag_start != std::string_view::npos && frag_start < path_end) {
            path_end = frag_start;
        }
        url.path_ = url_str.substr(pos, path_end - pos);
        pos = path_end;
    } else {
        url.path_ = "/";
    }

    // Parse query
    if (pos < url_str.length() && url_str[pos] == '?') {
        std::size_t query_end = url_str.length();
        if (frag_start != std::string_view::npos && frag_start < query_end) {
            query_end = frag_start;
        }
        url.query_ = url_str.substr(pos + 1, query_end - pos - 1);
        pos = query_end;
    }

    // Parse fragment
    if (pos < url_str.length() && url_str[pos] == '#') {
        url.fragment_ = url_str.substr(pos + 1);
    }

    return url;
}

std::string Url::full() const {
    std::string result;
    result.reserve(str_.size());
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
    result.reserve(scheme_.size() + host_.size() + port_.size() + 4);
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
    if (path_.empty() || path_ == "/") return "index.html";

    auto last_slash = path_.rfind('/');
    auto start = (last_slash == std::string_view::npos) ? 0 : last_slash + 1;

    auto query_pos = path_.find('?', start);
    auto frag_pos = path_.find('#', start);
    auto end = path_.length();

    if (query_pos != std::string_view::npos && query_pos < end) end = query_pos;
    if (frag_pos != std::string_view::npos && frag_pos < end) end = frag_pos;

    auto filename = path_.substr(start, end - start);

    if (filename.empty() || filename.ends_with('/')) return "index.html";

    return std::string(filename);
}

} // namespace bolt::core
