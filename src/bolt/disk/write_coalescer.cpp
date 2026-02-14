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

    // Merge adjacent/overlapping writes before flushing
    merge_writes();

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

void WriteCoalescer::merge_writes() noexcept {
    // Already holding lock from caller (flush)
    if (pending_.size() < 2) return;

    // Iterate through and merge adjacent/overlapping writes
    auto it = pending_.begin();
    while (it != pending_.end()) {
        auto next = std::next(it);
        if (next == pending_.end()) break;

        std::uint64_t it_end = it->first + it->second.data.size();
        std::uint64_t next_start = next->first;

        // Check if adjacent or overlapping
        if (it_end >= next_start) {
            // Merge next into current
            std::uint64_t merged_end = std::max(it_end, next_start + next->second.data.size());
            std::size_t new_size = static_cast<std::size_t>(merged_end - it->first);

            // Expand current buffer
            std::vector<std::byte> merged;
            merged.reserve(new_size);
            merged.insert(merged.end(), it->second.data.begin(), it->second.data.end());

            // If there's a gap, fill with zeros (shouldn't happen with proper enqueue)
            if (it_end < next_start) {
                merged.resize(next_start - it->first, std::byte{0});
            }

            // Append next data
            merged.insert(merged.end(), next->second.data.begin(), next->second.data.end());

            total_pending_ -= it->second.data.size();
            total_pending_ -= next->second.data.size();

            it->second.data = std::move(merged);
            total_pending_ += it->second.data.size();

            // Erase next and continue checking from current position
            pending_.erase(next);
            // Don't advance it - check if we can merge more
        } else {
            ++it;
        }
    }
}

} // namespace bolt::disk
