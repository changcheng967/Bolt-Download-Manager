// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/core/http_session.hpp>
#include <bolt/core/config.hpp>
#include <curl/curl.h>
#include <algorithm>
#include <regex>

namespace bolt::core {

namespace {

// RAII initializer for curl
struct CurlInitializer {
    CurlInitializer() { curl_global_init(CURL_GLOBAL_ALL); }
    ~CurlInitializer() { curl_global_cleanup(); }
};

// Header callback for HEAD/GET requests
std::size_t header_callback(char* buffer, std::size_t size, std::size_t nitems, void* userdata) {
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    std::size_t bytes = size * nitems;

    std::string_view header(buffer, bytes);

    // Skip empty lines and HTTP status line
    if (header.empty() || header[0] == '\r' || header[0] == '\n') {
        return bytes;
    }

    if (header.starts_with("HTTP/")) {
        return bytes;
    }

    // Parse "Name: Value"
    auto colon_pos = header.find(':');
    if (colon_pos != std::string_view::npos) {
        auto name = header.substr(0, colon_pos);
        auto value_start = colon_pos + 1;

        // Skip leading whitespace
        while (value_start < header.size() && (header[value_start] == ' ' || header[value_start] == '\t')) {
            ++value_start;
        }

        auto value = header.substr(value_start);

        // Trim trailing \r\n
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
            value.remove_suffix(1);
        }

        // Convert name to lowercase
        std::string lower_name;
        lower_name.reserve(name.size());
        for (char c : name) {
            lower_name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        (*headers)[std::string(lower_name)] = std::string(value);
    }

    return bytes;
}

// Discard callback for HEAD requests
std::size_t discard_callback(char*, std::size_t size, std::size_t nmemb, void*) {
    return size * nmemb;
}

} // namespace

//=============================================================================
// HttpSession
//=============================================================================

void HttpSession::global_init() noexcept {
    static CurlInitializer init;
}

void HttpSession::global_cleanup() noexcept {
    curl_global_cleanup();
}

HttpSession::HttpSession() = default;

HttpSession::~HttpSession() {
    cleanup_idle_connections();
}

HttpSession::HttpSession(HttpSession&& other) noexcept
    : connection_pool_(std::move(other.connection_pool_)) {}

HttpSession& HttpSession::operator=(HttpSession&& other) noexcept {
    if (this != &other) {
        cleanup_idle_connections();
        connection_pool_ = std::move(other.connection_pool_);
    }
    return *this;
}

std::expected<HttpResponse, std::error_code> HttpSession::head(const std::string& url) noexcept {
    auto* curl = curl_easy_init();
    if (!curl) {
        return std::unexpected(make_error_code(DownloadErrc::network_error));
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(MAX_REDIRECTS));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(CONNECTION_TIMEOUT_SEC));
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &connection_pool_["_temp_headers"]);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_callback);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    auto result = curl_easy_perform(curl);
    long http_code = 0;

    if (result == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    HttpResponse response = parse_response(curl);
    response.status_code = static_cast<std::int32_t>(http_code);

    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        return std::unexpected(make_error_code(DownloadErrc::network_error));
    }

    if (http_code >= 400) {
        if (http_code == 404) {
            return std::unexpected(make_error_code(DownloadErrc::not_found));
        }
        return std::unexpected(make_error_code(DownloadErrc::server_error));
    }

    return response;
}

std::expected<HttpResponse, std::error_code> HttpSession::get(const std::string& url,
                                                                std::uint64_t offset,
                                                                std::uint64_t size) noexcept {
    auto* curl = curl_easy_init();
    if (!curl) {
        return std::unexpected(make_error_code(DownloadErrc::network_error));
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(MAX_REDIRECTS));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(CONNECTION_TIMEOUT_SEC));
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &connection_pool_["_temp_headers"]);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_callback);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);

    if (size > 0) {
        std::string range = std::format("{}-{}", offset, offset + size - 1);
        curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
    }

    auto result = curl_easy_perform(curl);
    long http_code = 0;

    if (result == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    HttpResponse response = parse_response(curl);
    response.status_code = static_cast<std::int32_t>(http_code);

    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        return std::unexpected(make_error_code(DownloadErrc::network_error));
    }

    return response;
}

void* HttpSession::acquire_connection(const std::string& host) noexcept {
    auto& pool = connection_pool_[host];

    // Find idle connection
    for (auto& entry : pool) {
        if (!entry.in_use) {
            entry.in_use = true;
            entry.last_used = std::chrono::steady_clock::now();
            return entry.handle;
        }
    }

    // Create new connection
    CURL* curl = curl_easy_init();
    if (curl) {
        pool.push_back({curl, std::chrono::steady_clock::now(), true});
        return curl;
    }

    return nullptr;
}

void HttpSession::release_connection(const std::string& host, void* handle) noexcept {
    auto it = connection_pool_.find(host);
    if (it == connection_pool_.end()) return;

    for (auto& entry : it->second) {
        if (entry.handle == handle) {
            entry.in_use = false;
            entry.last_used = std::chrono::steady_clock::now();
            return;
        }
    }
}

void HttpSession::cleanup_idle_connections() noexcept {
    auto now = std::chrono::steady_clock::now();
    constexpr auto IDLE_TIMEOUT = std::chrono::seconds(60);

    for (auto& [host, pool] : connection_pool_) {
        auto it = std::remove_if(pool.begin(), pool.end(),
            [now](const ConnectionEntry& entry) {
                if (!entry.in_use) {
                    auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(
                        now - entry.last_used);
                    if (idle_time > IDLE_TIMEOUT) {
                        curl_easy_cleanup(static_cast<CURL*>(entry.handle));
                        return true;
                    }
                }
                return false;
            });
        pool.erase(it, pool.end());
    }
}

HttpResponse HttpSession::parse_response(void* curl) noexcept {
    HttpResponse response;

    // Get info from curl
    double cl = 0;
    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl) == CURLE_OK) {
        response.content_length = static_cast<std::uint64_t>(cl);
    }

    char* ct = nullptr;
    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct) == CURLE_OK && ct) {
        response.content_type = ct;
    }

    // Note: headers are in connection_pool_["_temp_headers"] which should be cleared
    // This is a simplified implementation - in full version we'd pass headers map directly

    response.accepts_ranges = true; // We'll probe this separately
    response.content_type = "application/octet-stream";

    return response;
}

std::string HttpSession::extract_filename(std::string_view content_disposition) noexcept {
    // Parse "attachment; filename=\"file.txt\"" or "attachment; filename=file.txt"
    static const std::regex filename_regex(R"(filename[*]?=["']?([^"'\r\n;]+)["']?)",
                                           std::regex::icase);

    std::string cd(content_disposition);
    std::smatch match;

    if (std::regex_search(cd, match, filename_regex) && match.size() > 1) {
        return match[1].str();
    }

    return {};
}

} // namespace bolt::core
