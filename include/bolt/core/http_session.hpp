// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/core/error.hpp>
#include <bolt/core/url.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <expected>
#include <map>
#include <vector>
#include <chrono>
#include <functional>

namespace bolt::core {

// HTTP response headers
struct HttpResponse {
    std::int32_t status_code{0};
    std::string status_message;
    std::map<std::string, std::string> headers;
    std::uint64_t content_length{0};
    bool accepts_ranges{false};
    std::string etag;
    std::string last_modified;
    std::string content_type;
    std::string filename; // From Content-Disposition
};

// Connection pool entry
struct ConnectionEntry {
    void* handle{nullptr}; // CURL*
    std::chrono::steady_clock::time_point last_used;
    bool in_use{false};
};

class HttpSession {
public:
    HttpSession();
    ~HttpSession();

    // Non-copyable, movable
    HttpSession(const HttpSession&) = delete;
    HttpSession& operator=(const HttpSession&) = delete;
    HttpSession(HttpSession&&) noexcept;
    HttpSession& operator=(HttpSession&&) noexcept;

    // Perform HEAD request to get file info
    [[nodiscard]] std::expected<HttpResponse, std::error_code>
    head(const std::string& url) noexcept;

    // Perform GET request
    [[nodiscard]] std::expected<HttpResponse, std::error_code>
    get(const std::string& url,
        std::uint64_t offset = 0,
        std::uint64_t size = 0) noexcept;

    // Get a connection from pool or create new one
    [[nodiscard]] void* acquire_connection(const std::string& host) noexcept;

    // Return connection to pool
    void release_connection(const std::string& host, void* handle) noexcept;

    // Close idle connections
    void cleanup_idle_connections() noexcept;

    // Global initialization (call once at startup)
    static void global_init() noexcept;
    static void global_cleanup() noexcept;

private:
    std::map<std::string, std::vector<ConnectionEntry>> connection_pool_;

    // Extract filename from headers
    static std::string extract_filename(const std::map<std::string, std::string>& headers) noexcept;

    // Parse Content-Disposition header value
    static std::string parse_content_disposition(std::string_view content_disposition) noexcept;
};

} // namespace bolt::core
