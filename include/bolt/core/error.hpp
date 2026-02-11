// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <system_error>
#include <string_view>

namespace bolt::core {

enum class DownloadErrc {
    success = 0,
    network_error,
    timeout,
    refused,
    not_found,
    server_error,
    disk_full,
    permission_denied,
    file_exists,
    invalid_url,
    invalid_range,
    checksum_mismatch,
    resume_failed,
    cancelled,
    no_bandwidth,
    stall_detected,
    too_many_redirects,
    ssl_error,
    dns_error,
    connection_lost,
};

namespace detail {

struct DownloadErrcCategory : std::error_category {
    [[nodiscard]] const char* name() const noexcept override {
        return "bolt::download";
    }

    [[nodiscard]] std::string message(int ev) const noexcept override {
        switch (static_cast<DownloadErrc>(ev)) {
            case DownloadErrc::success:              return "Success";
            case DownloadErrc::network_error:        return "Network error";
            case DownloadErrc::timeout:              return "Operation timed out";
            case DownloadErrc::refused:              return "Connection refused";
            case DownloadErrc::not_found:            return "Resource not found (404)";
            case DownloadErrc::server_error:         return "Server error (5xx)";
            case DownloadErrc::disk_full:            return "Disk full";
            case DownloadErrc::permission_denied:    return "Permission denied";
            case DownloadErrc::file_exists:          return "File already exists";
            case DownloadErrc::invalid_url:          return "Invalid URL";
            case DownloadErrc::invalid_range:        return "Invalid byte range";
            case DownloadErrc::checksum_mismatch:    return "Checksum mismatch";
            case DownloadErrc::resume_failed:        return "Resume failed";
            case DownloadErrc::cancelled:            return "Download cancelled";
            case DownloadErrc::no_bandwidth:         return "No bandwidth detected";
            case DownloadErrc::stall_detected:        return "Download stalled";
            case DownloadErrc::too_many_redirects:   return "Too many redirects";
            case DownloadErrc::ssl_error:            return "SSL/TLS error";
            case DownloadErrc::dns_error:            return "DNS resolution failed";
            case DownloadErrc::connection_lost:      return "Connection lost";
            default:                                 return "Unknown error";
        }
    }
};

} // namespace detail

inline const detail::DownloadErrcCategory& download_errc_category() noexcept {
    static detail::DownloadErrcCategory category;
    return category;
}

inline std::error_code make_error_code(DownloadErrc e) noexcept {
    return {static_cast<int>(e), download_errc_category()};
}

} // namespace bolt::core

namespace std {

template<>
struct is_error_code_enum<bolt::core::DownloadErrc> : true_type {};

} // namespace std
