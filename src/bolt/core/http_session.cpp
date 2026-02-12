// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/core/http_session.hpp>
#include <bolt/core/config.hpp>
#include <curl/curl.h>
#include <algorithm>
#include <cstdlib>
#include <string>

// Windows headers
#include <windows.h>

namespace bolt::core {

namespace {

constexpr std::size_t WRITE_BUFFER_SIZE = 256 * 1024;  // 256 KB

// RAII curl handle cleanup
struct CurlHandle {
    CURL* ptr = nullptr;

    CurlHandle() = default;
    explicit CurlHandle(CURL* c) : ptr(c) {}
    ~CurlHandle() { if (ptr) curl_easy_cleanup(ptr); }

    CurlHandle(const CurlHandle&) = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;
    CurlHandle(CurlHandle&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    CurlHandle& operator=(CurlHandle&& other) noexcept {
        if (this != &other) {
            if (ptr) curl_easy_cleanup(ptr);
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
};

// Header callback for HEAD/GET responses
std::size_t header_callback(char* buffer, std::size_t size, std::size_t nitems, void* userdata) {
    std::size_t total = size * nitems;
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    if (!headers) return total;

    std::string_view header(buffer, total);
    auto colon = header.find(':');
    if (colon == std::string_view::npos) return total;

    auto name = header.substr(0, colon);
    auto value = header.substr(colon + 1);

    // Trim whitespace and \r\n
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }

    // Remove trailing \r or \n
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
        value.remove_suffix(1);
    }

    // Convert name to lowercase
    std::string lower_name;
    lower_name.reserve(name.size());
    for (char c : name) {
        lower_name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    (*headers)[lower_name] = std::string(value);
    return total;
}

// Discard callback for HEAD requests
std::size_t discard_callback(char*, std::size_t size, std::size_t nitems, void*) {
    return size * nitems;
}

// Write callback for GET requests (stores data)
struct WriteData {
    std::vector<std::byte> buffer;
    std::uint64_t total_written{0};
    std::function<void(std::uint64_t, std::error_code)> callback;

    WriteData() = default;

    explicit WriteData(std::function<void(std::uint64_t, std::error_code)> cb)
        : callback(std::move(cb)) {
        buffer.reserve(WRITE_BUFFER_SIZE);
    }

    void reset() {
        buffer.clear();
        total_written = 0;
    }
};

std::size_t write_callback(char* ptr, std::size_t size, std::size_t nitems, void* userdata) {
    auto* wd = static_cast<WriteData*>(userdata);
    if (!wd) return 0;

    std::size_t total = size * nitems;
    const std::size_t offset = wd->buffer.size();

    // Reserve space
    wd->buffer.reserve(offset + total);
    wd->buffer.resize(offset + total);

    // Copy new data
    std::memcpy(wd->buffer.data() + offset, ptr, total);
    wd->total_written += total;

    // Call progress callback if set
    if (wd->callback) {
        wd->callback(wd->total_written, {});
    }

    return total;
}

} // namespace

//=============================================================================
// HttpSession
//=============================================================================

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

std::expected<HttpResponse, std::error_code>
HttpSession::head(const std::string& url) noexcept {
    CurlHandle curl = CurlHandle(curl_easy_init());
    if (!curl.ptr) {
        return std::unexpected(make_error_code(DownloadErrc::network_error));
    }

    HttpResponse response{};

    // Set URL
    curl_easy_setopt(curl.ptr, CURLOPT_URL, url.c_str());

    // HEAD request
    curl_easy_setopt(curl.ptr, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl.ptr, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.ptr, CURLOPT_MAXREDIRS, static_cast<long>(MAX_REDIRECTS));
    curl_easy_setopt(curl.ptr, CURLOPT_CONNECTTIMEOUT, static_cast<long>(CONNECTION_TIMEOUT_SEC));
    curl_easy_setopt(curl.ptr, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.ptr, CURLOPT_SSL_VERIFYHOST, 2L);

    // Set header callback BEFORE performing request
    curl_easy_setopt(curl.ptr, CURLOPT_HEADERFUNCTION, reinterpret_cast<void*>(header_callback));
    curl_easy_setopt(curl.ptr, CURLOPT_HEADERDATA, &response.headers);

    // Perform request
    CURLcode result = curl_easy_perform(curl.ptr);

    std::error_code ec;
    if (result != CURLE_OK) {
        ec = make_error_code(DownloadErrc::network_error);
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl.ptr, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code >= 400) {
            if (http_code == 404) {
                ec = make_error_code(DownloadErrc::not_found);
            } else if (http_code >= 500) {
                ec = make_error_code(DownloadErrc::server_error);
            } else if (http_code == 401 || http_code == 403) {
                ec = make_error_code(DownloadErrc::permission_denied);
            }
        }
    }

    // Get content length from headers (CURLINFO_CONTENT_LENGTH_DOWNLOAD_T doesn't work for HEAD)
    auto cl_it = response.headers.find("content-length");
    if (cl_it != response.headers.end() && !cl_it->second.empty()) {
        char* end = nullptr;
        unsigned long long val = std::strtoull(cl_it->second.c_str(), &end, 10);
        if (end == cl_it->second.c_str() + cl_it->second.size()) {
            response.content_length = static_cast<std::uint64_t>(val);
        } else {
            response.content_length = 0;
        }
    } else {
        response.content_length = 0;
    }

    // Get content type from headers
    auto ct_it = response.headers.find("content-type");
    if (ct_it != response.headers.end() && !ct_it->second.empty()) {
        response.content_type = ct_it->second;
    }

    // Get response code
    long http_code = 0;
    curl_easy_getinfo(curl.ptr, CURLINFO_RESPONSE_CODE, &http_code);
    response.status_code = static_cast<std::int32_t>(http_code);

    // Check if ranges are supported
    auto ar_it = response.headers.find("accept-ranges");
    response.accepts_ranges = (ar_it != response.headers.end() && ar_it->second.find("bytes") != std::string::npos);

    // Extract filename
    response.filename = extract_filename(response.headers);

    return ec ? std::unexpected(ec) : std::expected<HttpResponse, std::error_code>{response};
}

std::expected<HttpResponse, std::error_code>
HttpSession::get(const std::string& url,
                std::uint64_t offset,
                std::uint64_t size) noexcept {
    CurlHandle curl = CurlHandle(curl_easy_init());
    if (!curl.ptr) {
        return std::unexpected(make_error_code(DownloadErrc::network_error));
    }

    HttpResponse response{};

    // Set URL
    curl_easy_setopt(curl.ptr, CURLOPT_URL, url.c_str());

    // Range header
    if (size > 0) {
        std::string range = std::to_string(offset) + "-" + std::to_string(offset + size - 1);
        curl_easy_setopt(curl.ptr, CURLOPT_RANGE, range.c_str());
    }

    curl_easy_setopt(curl.ptr, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.ptr, CURLOPT_MAXREDIRS, static_cast<long>(MAX_REDIRECTS));
    curl_easy_setopt(curl.ptr, CURLOPT_CONNECTTIMEOUT, static_cast<long>(CONNECTION_TIMEOUT_SEC));
    curl_easy_setopt(curl.ptr, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl.ptr, CURLOPT_LOW_SPEED_TIME, static_cast<long>(STALL_TIMEOUT_SEC));
    curl_easy_setopt(curl.ptr, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.ptr, CURLOPT_SSL_VERIFYHOST, 2L);

    // HTTP/2
    curl_easy_setopt(curl.ptr, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);

    // Set header callback to capture headers
    curl_easy_setopt(curl.ptr, CURLOPT_HEADERFUNCTION, reinterpret_cast<void*>(header_callback));
    curl_easy_setopt(curl.ptr, CURLOPT_HEADERDATA, &response.headers);

    // Write function (discard data for now - Segment handles actual writing)
    curl_easy_setopt(curl.ptr, CURLOPT_WRITEFUNCTION, reinterpret_cast<void*>(discard_callback));
    curl_easy_setopt(curl.ptr, CURLOPT_WRITEDATA, nullptr);

    // Perform request
    CURLcode result = curl_easy_perform(curl.ptr);

    std::error_code ec;
    if (result != CURLE_OK) {
        ec = make_error_code(DownloadErrc::network_error);
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl.ptr, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code >= 400) {
            if (http_code == 416) {
                ec = make_error_code(DownloadErrc::invalid_range);
            } else if (http_code >= 500) {
                ec = make_error_code(DownloadErrc::server_error);
            } else {
                ec = make_error_code(DownloadErrc::network_error);
            }
        }
    }

    // Get content length from curl info (works for GET)
    curl_off_t cl = 0;
    if (curl_easy_getinfo(curl.ptr, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl) == CURLE_OK && cl > 0) {
        response.content_length = static_cast<std::uint64_t>(cl);
    }

    // Get content type
    char* ct = nullptr;
    if (curl_easy_getinfo(curl.ptr, CURLINFO_CONTENT_TYPE, &ct) == CURLE_OK && ct) {
        response.content_type = ct;
    }

    // Get response code
    long http_code = 0;
    curl_easy_getinfo(curl.ptr, CURLINFO_RESPONSE_CODE, &http_code);
    response.status_code = static_cast<std::int32_t>(http_code);

    return ec ? std::unexpected(ec) : std::expected<HttpResponse, std::error_code>{response};
}

void* HttpSession::acquire_connection(const std::string& host) noexcept {
    // Clean up old idle connections first
    cleanup_idle_connections();

    auto& pool = connection_pool_[host];

    // Find or create connection
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
        // Default options for pooled connections
        curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
        curl_easy_setopt(curl, CURLOPT_PIPEWAIT, 1L);

        ConnectionEntry entry;
        entry.handle = curl;
        entry.last_used = std::chrono::steady_clock::now();
        entry.in_use = true;
        pool.push_back(entry);
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
            return;
        }
    }
}

void HttpSession::cleanup_idle_connections() noexcept {
    auto now = std::chrono::steady_clock::now();
    constexpr std::chrono::seconds IDLE_TIMEOUT(60);

    for (auto& pair : connection_pool_) {
        auto& pool = pair.second;
        auto end = std::remove_if(pool.begin(), pool.end(),
            [&now, IDLE_TIMEOUT](const ConnectionEntry& e) {
                auto idle = std::chrono::duration_cast<std::chrono::seconds>(
                    now - e.last_used);
                return !e.in_use && idle > IDLE_TIMEOUT;
            });
        pool.erase(end, pool.end());
    }
}

std::string HttpSession::extract_filename(const std::map<std::string, std::string>& headers) noexcept {
    auto it = headers.find("content-disposition");
    if (it != headers.end() && !it->second.empty()) {
        return parse_content_disposition(it->second);
    }
    return {};
}

std::string HttpSession::parse_content_disposition(std::string_view content_disposition) noexcept {
    // Parse "attachment; filename=file.zip"
    auto filename_pos = content_disposition.find("filename=");
    if (filename_pos != std::string_view::npos) {
        auto filename = content_disposition.substr(filename_pos + 9);
        // Remove quotes if present
        if (filename.front() == '"' || filename.front() == '\'') {
            filename.remove_prefix(1);
            filename.remove_suffix(1);
        }
        return std::string(filename);
    }
    return {};
}

//=============================================================================
// Global CURL initialization
//=============================================================================

void HttpSession::global_init() noexcept {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void HttpSession::global_cleanup() noexcept {
    curl_global_cleanup();
}

} // namespace bolt::core
