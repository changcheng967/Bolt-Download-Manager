// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/core/download_engine.hpp>
#include <string>
#include <vector>
#include <expected>

namespace bolt::cli {

// CLI result
using CliResult = std::expected<int, std::error_code>;

// Command line arguments
struct CliArgs {
    std::vector<std::string> urls;
    std::string output_dir;
    std::string output_file;
    std::uint32_t segments{0};
    bool list_only{false};
    bool verbose{false};
    bool quiet{false};
    bool version{false};
    bool help{false};
};

// Parse command line arguments
[[nodiscard]] CliArgs parse_args(int argc, char* argv[]) noexcept;

// Download a single URL
[[nodiscard]] CliResult download(const std::string& url,
                                  const std::string& output,
                                  const std::string& output_dir,
                                  std::uint32_t segments,
                                  bool verbose,
                                  bool quiet) noexcept;

// Show help message
void print_help(std::string_view program_name) noexcept;

// Show version information
void print_version() noexcept;

// List download info without downloading
[[nodiscard]] CliResult info(const std::string& url) noexcept;

} // namespace bolt::cli
