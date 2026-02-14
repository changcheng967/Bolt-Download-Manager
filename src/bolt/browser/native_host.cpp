// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/browser/native_host.hpp>
#include <bolt/core/download_engine.hpp>
#include <bolt/core/error.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <io.h>
#include <fcntl.h>

namespace bolt::browser {

namespace {

// Parse JSON to DownloadRequest
std::expected<DownloadRequest, std::error_code> parse_request(const nlohmann::json& j) noexcept {
    try {
        DownloadRequest req;

        if (j.contains("url")) {
            req.url = j["url"].get<std::string>();
        }

        if (j.contains("filename")) {
            req.filename = j["filename"].get<std::string>();
        }

        if (j.contains("referrer")) {
            req.referrer = j["referrer"].get<std::string>();
        }

        if (j.contains("fileSize")) {
            req.file_size = j["fileSize"].get<std::uint64_t>();
        }

        if (j.contains("cookies") && j["cookies"].is_array()) {
            for (const auto& c : j["cookies"]) {
                req.cookies.push_back(c.get<std::string>());
            }
        }

        if (j.contains("headers") && j["headers"].is_object()) {
            for (auto& [key, value] : j["headers"].items()) {
                req.headers.emplace_back(key, value.get<std::string>());
            }
        }

        return req;
    } catch (const std::exception& e) {
        return std::unexpected(make_error_code(bolt::core::DownloadErrc::invalid_url));
    }
}

} // namespace

//=============================================================================
// NativeHost
//=============================================================================

int NativeHost::run() noexcept {
    // Set stdin/stdout to binary mode (Native messaging protocol is binary)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    bolt::core::DownloadEngine::global_init();

    // Main message loop
    while (true) {
        std::string msg = read_message();
        if (msg.empty()) {
            break;  // EOF
        }

        auto response = process_message(msg);
        if (response) {
            DownloadResponse resp = *response;
            send_response(resp);
        } else {
            send_response(DownloadResponse{false, "Failed to process request", 0});
        }
    }

    bolt::core::DownloadEngine::global_cleanup();
    return 0;
}

std::expected<DownloadResponse, std::error_code>
NativeHost::process_message(const std::string& json) noexcept {
    try {
        auto j = nlohmann::json::parse(json);

        auto req = parse_request(j);
        if (!req) {
            return DownloadResponse{false, "Invalid request", 0};
        }

        auto id = add_download(*req);
        if (!id) {
            return DownloadResponse{false, id.error().message(), 0};
        }

        return DownloadResponse{true, "Download added", *id};

    } catch (const std::exception& e) {
        return std::unexpected(make_error_code(bolt::core::DownloadErrc::invalid_url));
    }
}

void NativeHost::send_response(const DownloadResponse& response) noexcept {
    nlohmann::json j;
    j["success"] = response.success;
    j["message"] = response.message;
    j["downloadId"] = response.download_id;

    write_message(j.dump());
}

std::expected<std::uint32_t, std::error_code>
NativeHost::add_download(const DownloadRequest& request) noexcept {
    auto& manager = core::DownloadManager::instance();

    auto result = manager.create_download(request.url, request.filename);
    if (!result) {
        return std::unexpected(result.error());
    }

    // Start the download
    manager.start(*result);

    return *result;
}

std::string NativeHost::read_message() noexcept {
    // Read 4-byte length prefix
    std::uint32_t length = 0;
    std::cin.read(reinterpret_cast<char*>(&length), 4);

    if (std::cin.eof() || std::cin.fail()) {
        return "";
    }

    // Read JSON payload
    std::string buffer(length, '\0');
    std::cin.read(&buffer[0], length);

    return buffer;
}

void NativeHost::write_message(const std::string& json) noexcept {
    std::uint32_t length = static_cast<std::uint32_t>(json.size());

    std::cout.write(reinterpret_cast<const char*>(&length), 4);
    std::cout << json;
    std::cout.flush();
}

} // namespace bolt::browser

//=============================================================================
// Main entry point
//=============================================================================

int main() {
    bolt::browser::NativeHost host;
    return host.run();
}
