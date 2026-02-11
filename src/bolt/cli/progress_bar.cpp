// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/cli/progress_bar.hpp>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <format>

namespace bolt::cli {

namespace {

const char* SPINNER_FRAMES[] = {"-", "\\", "|", "/"};

} // namespace

//=============================================================================
// ProgressBar
//=============================================================================

ProgressBar::ProgressBar(std::uint64_t total, std::string_view label)
    : total_(total)
    , label_(label) {}

void ProgressBar::update(std::uint64_t current, std::uint64_t speed_bps) noexcept {
    if (total_ == 0) return;

    double percent = static_cast<double>(current) * 100.0 / static_cast<double>(total_);
    percent = std::clamp(percent, 0.0, 100.0);

    // Only redraw if significant progress (every 1%)
    auto scaled = static_cast<std::uint64_t>(percent);
    auto last_scaled = static_cast<std::uint64_t>(
        static_cast<double>(last_drawn_) * 100.0 / static_cast<double>(total_));

    if (scaled <= last_scaled && !finished_) return;

    last_drawn_ = current;

    std::string bar = render_bar(percent);

    // Build status line
    std::string line = "\r";
    if (!label_.empty()) {
        line += label_ + ": ";
    }

    line += bar;
    line += std::format(" {:>5.1f}%", percent);
    line += std::format(" ({}/{})", format_bytes(current), format_bytes(total_));

    if (speed_bps > 0) {
        line += std::format(" @ {}", format_speed(speed_bps));

        // ETA
        std::uint64_t remaining = total_ - current;
        if (speed_bps > 0) {
            std::uint64_t eta = remaining / speed_bps;
            line += std::format(" ETA: {}", format_time(eta));
        }
    }

    // Clear rest of line
    line += std::string(10, ' ');

    std::print("{}", line);
    std::fflush(stdout);
}

void ProgressBar::finish() noexcept {
    if (finished_) return;
    finished_ = true;
    update(total_, 0);
    std::println("");
}

void ProgressBar::clear() noexcept {
    std::print("\r{: <{}}\r", "", width_ + 50);
}

std::string ProgressBar::render_bar(double percent) const noexcept {
    const int filled = static_cast<int>(std::round(width_ * percent / 100.0));
    const int empty = width_ - filled;

    std::string bar = "[";
    bar.append(filled, '=');
    bar.append(1, '>');
    bar.append(empty, ' ');
    bar += "]";

    return bar;
}

std::string ProgressBar::format_speed(std::uint64_t bps) noexcept {
    constexpr std::uint64_t KB = 1024;
    constexpr std::uint64_t MB = 1024 * KB;
    constexpr std::uint64_t GB = 1024 * MB;

    if (bps >= GB) {
        return std::format("{:.1f} GB/s", static_cast<double>(bps) / GB);
    } else if (bps >= MB) {
        return std::format("{:.1f} MB/s", static_cast<double>(bps) / MB);
    } else if (bps >= KB) {
        return std::format("{:.1f} KB/s", static_cast<double>(bps) / KB);
    }
    return std::format("{} B/s", bps);
}

std::string ProgressBar::format_bytes(std::uint64_t bytes) noexcept {
    constexpr std::uint64_t KB = 1024;
    constexpr std::uint64_t MB = 1024 * KB;
    constexpr std::uint64_t GB = 1024 * MB;
    constexpr std::uint64_t TB = 1024 * GB;

    if (bytes >= TB) {
        return std::format("{:.2f} TB", static_cast<double>(bytes) / TB);
    } else if (bytes >= GB) {
        return std::format("{:.2f} GB", static_cast<double>(bytes) / GB);
    } else if (bytes >= MB) {
        return std::format("{:.2f} MB", static_cast<double>(bytes) / MB);
    } else if (bytes >= KB) {
        return std::format("{:.2f} KB", static_cast<double>(bytes) / KB);
    }
    return std::format("{} B", bytes);
}

std::string ProgressBar::format_time(std::uint64_t seconds) noexcept {
    if (seconds == 0) return "0s";

    std::uint64_t hours = seconds / 3600;
    std::uint64_t minutes = (seconds % 3600) / 60;
    std::uint64_t secs = seconds % 60;

    if (hours > 0) {
        return std::format("{}h {}m", hours, minutes);
    } else if (minutes > 0) {
        return std::format("{}m {}s", minutes, secs);
    }
    return std::format("{}s", secs);
}

//=============================================================================
// Spinner
//=============================================================================

Spinner::Spinner(Style style) noexcept
    : style_(style) {}

void Spinner::update() noexcept {
    std::cout << "\r" << SPINNER_FRAMES[frame_ % 4] << " " << std::flush;
    ++frame_;
}

void Spinner::finish() noexcept {
    std::cout << "\r done" << std::endl;
}

void Spinner::clear() noexcept {
    std::cout << "\r      \r" << std::flush;
}

} // namespace bolt::cli
