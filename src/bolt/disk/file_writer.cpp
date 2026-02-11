// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/disk/file_writer.hpp>
#include <bolt/core/config.hpp>

namespace bolt::disk {

//=============================================================================
// FileWriter
//=============================================================================

std::error_code FileWriter::open(std::string_view path, std::uint64_t size) noexcept {
    auto lock = std::lock_guard(mutex_);

    close();
    path_ = path;

    auto file = AsyncFile::open(path, size);
    if (!file) {
        return file.error();
    }

    file_ = std::make_unique<AsyncFile>(std::move(*file));
    return {};
}

std::error_code FileWriter::write(std::uint64_t offset,
                                     const void* data,
                                     std::size_t size) noexcept {
    auto lock = std::lock_guard(mutex_);

    if (!file_) {
        return make_error_code(DiskErrc::handle_invalid);
    }

    auto result = file_->write(offset, data, size);
    return result ? std::error_code{} : result.error();
}

void FileWriter::write_async(std::uint64_t offset,
                              std::vector<std::byte> data,
                              std::function<void(std::error_code)> callback) noexcept {
    // For now, execute synchronously
    auto ec = write(offset, data.data(), data.size());
    if (callback) {
        callback(ec);
    }
}

std::error_code FileWriter::flush() noexcept {
    auto lock = std::lock_guard(mutex_);

    if (!file_) {
        return make_error_code(DiskErrc::handle_invalid);
    }

    return file_->flush();
}

void FileWriter::close() noexcept {
    auto lock = std::lock_guard(mutex_);

    if (file_) {
        file_->flush();
        file_->close();
        file_.reset();
    }
}

//=============================================================================
// SegmentBuffer
//=============================================================================

SegmentBuffer::SegmentBuffer(std::size_t capacity) {
    buffer_.resize(capacity);
}

void SegmentBuffer::append(const void* data, std::size_t size) noexcept {
    if (size_ + size > buffer_.size()) {
        buffer_.resize(size_ + size);
    }

    std::memcpy(buffer_.data() + size_, data, size);
    size_ += size;
}

} // namespace bolt::disk
