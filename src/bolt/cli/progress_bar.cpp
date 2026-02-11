// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/cli/progress_bar.hpp>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iomanip>

namespace bolt::cli {

namespace {

const char* SPINNER_FRAMES[] = {"-", "\\", "|", "/"};

} // namespace

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

    // Build status line
    std::string line = "\r";
    if (!label_.empty()) {
        line += label_;
        line += ": ";
    }

    // Build progress bar
    constexpr int bar_width = 30;
    const int filled = static_cast<int>(std::round(bar_width * percent / 100.0));
    const int empty = bar_width - filled;

    std::string bar = "[";
    for (int i = 0; i < filled; ++i) {
        bar += '=';
    }
    bar += ">";
    for (int i = 0; i < empty; ++i) {
        bar += ' ';
    }

    line += bar;

    // Add percentage
    line += " ";
    // Format percentage with padding
    int pct_int = static_cast<int>(percent);
    if (pct_int < 10) line += " ";
    if (pct_int < 100) line += std::to_string(pct_int) + "%";

    // Add fraction
    line += " (";
    line += format_bytes(current);
    line += "/";
    line += format_bytes(total_);
    line += ")";

    // Add speed
    if (speed_bps > 0) {
        line += " @ ";
        line += format_speed(speed_bps);
    }

    // ETA
    std::uint64_t remaining = total_ - current;
    if (speed_bps > 0 && remaining > 0) {
        std::uint64_t eta = remaining / speed_bps;
        line += " ETA: ";
        line += format_time(eta);
    }

    // Clear rest of line
    line += std::string(10, ' ');

    std::cout << line << std::flush;
}

void ProgressBar::finish() noexcept {
    if (finished_) return;
    finished_ = true;
    update(total_, 0);
    std::cout << std::endl;
}

void ProgressBar::clear() noexcept {
    std::cout << "\r" << std::string(50, ' ') << "\r" << std::flush;
}

std::string ProgressBar::render_bar(double percent) const noexcept {
    const int bar_width = 30;
    const int filled = static_cast<int>(std::round(bar_width * percent / 100.0));
    const int empty = bar_width - filled;

    std::string bar = "[";
    for (int i = 0; i < filled; ++i) {
        bar += '=';
    }
    bar.append(1, '>');
    for (int i = 0; i < empty; ++i) {
        bar.append(' ');
    }
    bar += "]";
    return bar;
}

std::string ProgressBar::format_speed(std::uint64_t bps) noexcept {
    constexpr std::uint64_t KB = 1024;
    constexpr std::uint64_t MB = 1024 * KB;
    constexpr std::uint64_t GB = 1024 * MB;

    if (bps >= GB) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (static_cast<double>(bps) / GB);
        return ss.str() + " GB/s";
    } else if (bps >= MB) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (static_cast<double>(bps) / MB);
        return ss.str() + " MB/s";
    } else if (bps >= KB) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (static_cast<double>(bps) / KB);
        return ss.str() + " KB/s";
    }
    return std::to_string(bps) + " B/s";
}

std::string ProgressBar::format_bytes(std::uint64_t bytes) noexcept {
    constexpr std::uint64_t KB = 1024;
    constexpr std::uint64_t MB = 1024 * KB;
    constexpr std::uint64_t GB = 1024 * MB;
    constexpr std::uint64_t TB = 1024 * GB;

    if (bytes >= TB) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / TB);
        return ss.str() + " TB";
    } else if (bytes >= GB) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / GB);
        return ss.str() + " GB";
    } else if (bytes >= MB) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / MB);
        return ss.str() + " MB";
    } else if (bytes >= KB) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(0) << (static_cast<double>(bytes) / KB);
        return ss.str() + " KB";
    }
    return std::to_string(bytes) + " B";
}

std::string ProgressBar::format_time(std::uint64_t seconds) noexcept {
    std::uint64_t hours = seconds / 3600;
    std::uint64_t minutes = (seconds % 3600) / 60;
    std::uint64_t secs = seconds % 60;

    std::ostringstream ss;
    if (hours > 0) {
        ss << hours << "h " << std::setfill('0') << std::setw(2) << minutes;
        return ss.str() + "m " + std::to_string(secs) + "s";
    } else if (minutes > 0) {
        return std::to_string(minutes) + "m " + std::to_string(secs) + "s";
    }
    return std::to_string(secs) + "s";
}

} // namespace bolt::cli
