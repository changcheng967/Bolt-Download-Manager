// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/disk/write_coalescer.hpp>
#include <algorithm>

namespace bolt::disk {

WriteCoalescer::WriteCoalescer(std::uint64_t max_pending)
    : max_pending_(max_pending) {}

void WriteCoalescer::enqueue(std::uint64_t offset,
                              std::vector<std::byte> data) noexcept {
    auto lock = std::lock_guard(mutex_);

    // Check for overlap with existing writes
    auto it = pending_.upper_bound(offset);
    if (it != pending_.begin()) {
        --it;
        if (it->first + it->second.data.size() > offset) {
            // Overlap - merge
            auto overlap_end = std::max(offset + data.size(),
                                        it->first + it->second.data.size());
            it->second.data.resize(overlap_end - it->first);

            total_pending_ -= it->second.data.size();
            std::memcpy(it->second.data.data() + (offset - it->first),
                        data.data(), data.size());
            total_pending_ += it->second.data.size();
            return;
        }
    }

    // Check if next write is adjacent
    auto next = pending_.upper_bound(offset);
    if (next != pending_.end() &&
        offset + data.size() == next->first) {
        // Merge into next
        std::size_t new_size = data.size() + next->second.data.size();
        total_pending_ -= next->second.data.size();

        std::vector<std::byte> merged;
        merged.reserve(new_size);
        merged.insert(merged.end(), data.begin(), data.end());
        merged.insert(merged.end(), next->second.data.begin(),
                       next->second.data.end());

        pending_.erase(next);
        pending_[offset] = {offset, std::move(merged)};
        total_pending_ += new_size;
        return;
    }

    // No overlap - add as new entry
    total_pending_ += data.size();
    pending_[offset] = {offset, std::move(data)};
}

std::error_code WriteCoalescer::flush(AsyncFile& file) noexcept {
    auto lock = std::lock_guard(mutex_);

    for (auto& [offset, write] : pending_) {
        auto result = file.write(write.offset, write.data.data(), write.data.size());
        if (!result) {
            return result.error();
        }
    }

    pending_.clear();
    total_pending_ = 0;
    return {};
}

void WriteCoalescer::cancel() noexcept {
    auto lock = std::lock_guard(mutex_);
    pending_.clear();
    total_pending_ = 0;
}

std::uint64_t WriteCoalescer::pending_bytes() const noexcept {
    auto lock = std::lock_guard(mutex_);
    return total_pending_;
}

std::size_t WriteCoalescer::pending_count() const noexcept {
    auto lock = std::lock_guard(mutex_);
    return pending_.size();
}

} // namespace bolt::disk
