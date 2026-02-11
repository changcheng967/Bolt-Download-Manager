// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <string>
#include <vector>
#include <expected>
#include <system_error>

namespace bolt::browser {

// Native messaging protocol for browser integration
// Compatible with Chrome and Firefox Native Messaging API

struct DownloadRequest {
    std::string url;
    std::string filename;
    std::string referrer;
    std::uint64_t file_size{0};
    std::vector<std::string> cookies;
    std::vector<std::pair<std::string, std::string>> headers;
};

struct DownloadResponse {
    bool success{false};
    std::string message;
    std::uint32_t download_id{0};
};

// Native messaging host
class NativeHost {
public:
    NativeHost() = default;

    // Run the native messaging loop
    [[nodiscard]] int run() noexcept;

    // Process incoming message
    [[nodiscard]] std::expected<DownloadResponse, std::error_code>
    process_message(const std::string& json) noexcept;

    // Send response to browser
    void send_response(const DownloadResponse& response) noexcept;

    // Add download to BoltDM
    [[nodiscard]] std::expected<std::uint32_t, std::error_code>
    add_download(const DownloadRequest& request) noexcept;

private:
    // Read message from stdin (length-prefixed JSON)
    [[nodiscard]] std::string read_message() noexcept;

    // Write message to stdout (length-prefixed JSON)
    void write_message(const std::string& json) noexcept;

    std::uint32_t next_id_{1};
};

} // namespace bolt::browser
