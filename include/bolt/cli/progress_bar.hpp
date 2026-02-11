// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace bolt::cli {

// Minimal progress bar for CLI
class ProgressBar {
public:
    ProgressBar(std::uint64_t total, std::string_view label = {});

    // Update progress
    void update(std::uint64_t current, std::uint64_t speed_bps = 0) noexcept;

    // Finish the progress bar
    void finish() noexcept;

    // Clear the progress bar line
    void clear() noexcept;

    [[nodiscard]] std::uint64_t total() const noexcept { return total_; }
    void total(std::uint64_t t) noexcept { total_ = t; }

    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    void label(std::string_view l) noexcept { label_ = l; }

    // Enable/disable smooth animation
    void smooth(bool enable) noexcept { smooth_ = enable; }

private:
    [[nodiscard]] std::string render_bar(double percent) const noexcept;
    [[nodiscard]] static std::string format_speed(std::uint64_t bps) noexcept;
    [[nodiscard]] static std::string format_bytes(std::uint64_t bytes) noexcept;
    [[nodiscard]] static std::string format_time(std::uint64_t seconds) noexcept;

    std::uint64_t total_{0};
    std::uint64_t last_drawn_{0};
    std::string label_;
    bool smooth_{true};
    bool finished_{false};
    int width_{60};
};

// Spinner for indeterminate progress
class Spinner {
public:
    enum class Style { dots, line, arrow };

    explicit Spinner(Style style = Style::line) noexcept;

    void update() noexcept;
    void finish() noexcept;
    void clear() noexcept;

private:
    std::size_t frame_{0};
    Style style_;
};

} // namespace bolt::cli
