// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/disk/async_file.hpp>
#include <bolt/core/config.hpp>

namespace bolt::disk {

namespace {

std::error_code win32_error_to_error_code(DWORD win32_error) noexcept {
    switch (win32_error) {
        case ERROR_FILE_NOT_FOUND:     return make_error_code(DiskErrc::file_not_found);
        case ERROR_ACCESS_DENIED:      return make_error_code(DiskErrc::access_denied);
        case ERROR_DISK_FULL:          return make_error_code(DiskErrc::disk_full);
        case ERROR_INVALID_DRIVE:
        case ERROR_INVALID_NAME:       return make_error_code(DiskErrc::invalid_path);
        case ERROR_FILE_EXISTS:        return make_error_code(DiskErrc::file_exists);
        case ERROR_WRITE_FAULT:        return make_error_code(DiskErrc::write_error);
        case ERROR_READ_FAULT:         return make_error_code(DiskErrc::read_error);
        case ERROR_LOCK_VIOLATION:
        case ERROR_SHARING_VIOLATION:  return make_error_code(DiskErrc::lock_error);
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:        return make_error_code(DiskErrc::allocation_failed);
        case ERROR_INVALID_HANDLE:     return make_error_code(DiskErrc::handle_invalid);
        default:                       return make_error_code(DiskErrc::write_error);
    }
}

} // namespace

//=============================================================================
// AsyncFile
//=============================================================================

std::expected<AsyncFile, std::error_code>
AsyncFile::open(std::string_view path, std::uint64_t size) noexcept {
    AsyncFile file;
    file.path_ = path;

    // Convert to Windows path
    std::wstring wpath;
    wpath.reserve(path.size());
    for (char c : path) {
        wpath += static_cast<wchar_t>(c);
    }

    // Create file for async write
    DWORD flags = FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN;

    file.handle_ = CreateFileW(
        wpath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        flags,
        nullptr
    );

    if (file.handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(win32_error_to_error_code(GetLastError()));
    }

    // Pre-allocate if size specified
    if (size > 0) {
        auto result = file.pre_allocate(size);
        if (!result) {
            file.close();
            return std::unexpected(result);
        }
    }

    return file;
}

AsyncFile::~AsyncFile() {
    close();
}

AsyncFile::AsyncFile(AsyncFile&& other) noexcept
    : handle_(other.handle_)
    , path_(std::move(other.path_)) {
    other.handle_ = INVALID_HANDLE_VALUE;
}

AsyncFile& AsyncFile::operator=(AsyncFile&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        path_ = std::move(other.path_);
        other.handle_ = INVALID_HANDLE_VALUE;
    }
    return *this;
}

std::error_code AsyncFile::pre_allocate(std::uint64_t size) noexcept {
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(size);

    if (!SetFilePointerEx(handle_, li, nullptr, FILE_BEGIN)) {
        return win32_error_to_error_code(GetLastError());
    }

    if (!SetEndOfFile(handle_)) {
        return win32_error_to_error_code(GetLastError());
    }

    // Reset to beginning
    li.QuadPart = 0;
    SetFilePointerEx(handle_, li, nullptr, FILE_BEGIN);

    return {};
}

void AsyncFile::async_write(std::uint64_t offset,
                             const void* data,
                             std::size_t size,
                             AsyncCallback callback) noexcept {
    // For simplicity, use sync write for now
    // Full async implementation would use WriteFileEx with I/O completion ports
    auto result = write(offset, data, size);
    if (callback) {
        callback(result.value_or(0), result ? std::error_code{} : result.error());
    }
}

void AsyncFile::async_read(std::uint64_t offset,
                           void* buffer,
                           std::size_t size,
                           AsyncCallback callback) noexcept {
    auto result = read(offset, buffer, size);
    if (callback) {
        callback(result.value_or(0), result ? std::error_code{} : result.error());
    }
}

std::expected<std::size_t, std::error_code>
AsyncFile::write(std::uint64_t offset, const void* data, std::size_t size) noexcept {
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(offset);

    OVERLAPPED overlapped = {};
    overlapped.Offset = li.LowPart;
    overlapped.OffsetHigh = li.HighPart;

    DWORD bytes_written = 0;

    if (!WriteFile(handle_, data, static_cast<DWORD>(size),
                   &bytes_written, &overlapped)) {
        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            // Wait for completion
            if (!GetOverlappedResult(handle_, &overlapped, &bytes_written, TRUE)) {
                return std::unexpected(win32_error_to_error_code(GetLastError()));
            }
        } else {
            return std::unexpected(win32_error_to_error_code(error));
        }
    }

    return static_cast<std::size_t>(bytes_written);
}

std::expected<std::size_t, std::error_code>
AsyncFile::read(std::uint64_t offset, void* buffer, std::size_t size) noexcept {
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(offset);

    OVERLAPPED overlapped = {};
    overlapped.Offset = li.LowPart;
    overlapped.OffsetHigh = li.HighPart;

    DWORD bytes_read = 0;

    if (!ReadFile(handle_, buffer, static_cast<DWORD>(size),
                  &bytes_read, &overlapped)) {
        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            if (!GetOverlappedResult(handle_, &overlapped, &bytes_read, TRUE)) {
                return std::unexpected(win32_error_to_error_code(GetLastError()));
            }
        } else {
            return std::unexpected(win32_error_to_error_code(error));
        }
    }

    return static_cast<std::size_t>(bytes_read);
}

std::error_code AsyncFile::flush() noexcept {
    return FlushFileBuffers(handle_)
        ? std::error_code{}
        : win32_error_to_error_code(GetLastError());
}

void AsyncFile::close() noexcept {
    if (handle_ != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(handle_);
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}

//=============================================================================
// MappedFile
//=============================================================================

std::expected<MappedFile, std::error_code>
MappedFile::create(std::string_view path, std::uint64_t size) noexcept {
    MappedFile mf;
    mf.path_ = path;
    mf.size_ = size;

    std::wstring wpath;
    wpath.reserve(path.size());
    for (char c : path) {
        wpath += static_cast<wchar_t>(c);
    }

    // Create file
    mf.file_ = CreateFileW(
        wpath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (mf.file_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(win32_error_to_error_code(GetLastError()));
    }

    // Create file mapping
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(size);

    mf.mapping_ = CreateFileMappingW(
        mf.file_,
        nullptr,
        PAGE_READWRITE,
        li.HighPart,
        li.LowPart,
        nullptr
    );

    if (!mf.mapping_) {
        mf.close();
        return std::unexpected(win32_error_to_error_code(GetLastError()));
    }

    // Map view of file
    mf.data_ = MapViewOfFile(
        mf.mapping_,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        0
    );

    if (!mf.data_) {
        mf.close();
        return std::unexpected(win32_error_to_error_code(GetLastError()));
    }

    return mf;
}

MappedFile::~MappedFile() {
    close();
}

MappedFile::MappedFile(MappedFile&& other) noexcept
    : file_(other.file_)
    , mapping_(other.mapping_)
    , data_(other.data_)
    , size_(other.size_)
    , path_(std::move(other.path_)) {
    other.file_ = INVALID_HANDLE_VALUE;
    other.mapping_ = nullptr;
    other.data_ = nullptr;
    other.size_ = 0;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        close();
        file_ = other.file_;
        mapping_ = other.mapping_;
        data_ = other.data_;
        size_ = other.size_;
        path_ = std::move(other.path_);
        other.file_ = INVALID_HANDLE_VALUE;
        other.mapping_ = nullptr;
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

std::error_code MappedFile::write(std::uint64_t offset,
                                   const void* data,
                                   std::size_t size) noexcept {
    if (offset + size > size_) {
        return make_error_code(DiskErrc::write_error);
    }

    auto* dst = static_cast<std::byte*>(data_) + offset;
    std::memcpy(dst, data, size);
    return {};
}

std::error_code MappedFile::read(std::uint64_t offset,
                                  void* buffer,
                                  std::size_t size) noexcept {
    if (offset + size > size_) {
        return make_error_code(DiskErrc::read_error);
    }

    auto* src = static_cast<std::byte*>(data_) + offset;
    std::memcpy(buffer, src, size);
    return {};
}

std::error_code MappedFile::flush() noexcept {
    return FlushViewOfFile(data_, 0)
        ? std::error_code{}
        : win32_error_to_error_code(GetLastError());
}

void MappedFile::close() noexcept {
    if (data_) {
        FlushViewOfFile(data_, 0);
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }
    if (mapping_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }
    if (file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
}

} // namespace bolt::disk
