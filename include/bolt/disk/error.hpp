// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <system_error>

namespace bolt::disk {

enum class DiskErrc {
    success = 0,
    file_not_found,
    access_denied,
    disk_full,
    invalid_path,
    file_exists,
    write_error,
    read_error,
    seek_error,
    lock_error,
    allocation_failed,
    handle_invalid,
};

namespace detail {

struct DiskErrcCategory : std::error_category {
    [[nodiscard]] const char* name() const noexcept override {
        return "bolt::disk";
    }

    [[nodiscard]] std::string message(int ev) const noexcept override {
        switch (static_cast<DiskErrc>(ev)) {
            case DiskErrc::success:            return "Success";
            case DiskErrc::file_not_found:     return "File not found";
            case DiskErrc::access_denied:      return "Access denied";
            case DiskErrc::disk_full:          return "Disk full";
            case DiskErrc::invalid_path:       return "Invalid path";
            case DiskErrc::file_exists:        return "File already exists";
            case DiskErrc::write_error:        return "Write error";
            case DiskErrc::read_error:         return "Read error";
            case DiskErrc::seek_error:         return "Seek error";
            case DiskErrc::lock_error:         return "Lock error";
            case DiskErrc::allocation_failed:  return "Allocation failed";
            case DiskErrc::handle_invalid:     return "Invalid handle";
            default:                           return "Unknown error";
        }
    }
};

} // namespace detail

inline const detail::DiskErrcCategory& disk_errc_category() noexcept {
    static detail::DiskErrcCategory category;
    return category;
}

inline std::error_code make_error_code(DiskErrc e) noexcept {
    return {static_cast<int>(e), disk_errc_category()};
}

} // namespace bolt::disk

namespace std {

template<>
struct is_error_code_enum<bolt::disk::DiskErrc> : true_type {};

} // namespace std
