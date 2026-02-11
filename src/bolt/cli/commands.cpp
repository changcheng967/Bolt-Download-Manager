// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/cli/commands.hpp>
#include <bolt/cli/compat.hpp>
#include <bolt/version.hpp>
#include <iostream>
#include <algorithm>

using namespace bolt::cli;
using namespace bolt::compat;  // For println, format

namespace {

const char* SPINNER_FRAMES[] = {"-", "\\", "|", "/"};

} // namespace

//=============================================================================
// Argument parsing
//=============================================================================

CliArgs parse_args(int argc, char* argv[]) noexcept {
    CliArgs args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            args.help = true;
            return args;
        }
        if (arg == "-v" || arg == "--version") {
            args.version = true;
            return args;
        }
        if (arg == "-V" || arg == "--verbose") {
            args.verbose = true;
            continue;
        }
        if (arg == "-q" || arg == "--quiet") {
            args.quiet = true;
            continue;
        }
        if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                args.output_file = argv[++i];
            }
            continue;
        }
        if (arg == "-d" || arg == "--directory") {
            if (i + 1 < argc) {
                args.output_dir = argv[++i];
            }
            continue;
        }
        if (arg == "-n" || arg == "--segments") {
            if (i + 1 < argc) {
                args.segments = std::stoul(argv[++i]);
            }
            continue;
        }
        if (arg == "-i" || arg == "--info") {
            args.list_only = true;
            continue;
        }
        // Assume it's a URL
        if (arg.starts_with("http://") || arg.starts_with("https://")) {
            args.urls.push_back(arg);
        }
    }
    return args;
}

//=============================================================================
// Commands
//=============================================================================

CliResult download(const std::string& url,
                   const std::string& output,
                   std::uint32_t segments,
                   bool verbose,
                   bool quiet) noexcept {
    DownloadEngine::global_init();

    std::unique_ptr<DownloadEngine> engine = std::make_unique<DownloadEngine>();

    if (verbose) compat::println(std::cout, "Setting URL: ", url);

    auto result = engine->set_url(url);
    if (!result) {
        compat::println(std::cerr, "Error: Invalid URL: ", result.error().message());
        return std::unexpected(result.error());
    }

    if (!output.empty()) {
        engine->output_path(output);
        if (verbose) compat::println(std::cout, "Output path: ", output);
    }

    // Configure
    DownloadConfig config;
    if (segments > 0) {
        config.max_segments = segments;
        config.min_segments = std::min(segments, 2u);
    } else {
        config.max_segments = 2;   // Auto
        config.min_segments = 2;
        config.auto_segment = true;
    }
    engine->config(config);

    // Setup progress bar
    ProgressBar bar(0, "Downloading");
    if (!quiet) {
        engine->callback([&](const DownloadProgress& p) {
            bar.total(p.total_bytes);
            bar.update(p.downloaded_bytes, p.speed_bps);

            if (verbose) {
                auto progress = engine->progress();
                compat::println(std::cout,
                    "Active: ", progress.active_segments,
                    " Completed: ", progress.completed_segments,
                    " Speed: ", compat::format(progress.speed_bps));
            }
        });
    }

    if (verbose) compat::println(std::cout, "Starting download...");

    auto start_result = engine->start();
    if (start_result) {
        compat::println(std::cerr, "Error: Failed to start download: ", start_result.message());
        DownloadEngine::global_cleanup();
        return std::unexpected(start_result.error());
    }

    // Wait for completion
    while (true) {
        auto state = engine->state();
        if (state == DownloadState::completed) {
            if (!quiet) bar.finish();
            if (verbose) compat::println(std::cout, "Download completed!");
            break;
        }
        if (state == DownloadState::failed) {
            if (!quiet) bar.clear();
            compat::println(std::cerr, "Error: Download failed");
            DownloadEngine::global_cleanup();
            return std::unexpected(make_error_code(DownloadErrc::network_error));
        }
        if (state == DownloadState::cancelled) {
            if (!quiet) bar.clear();
            compat::println(std::cerr, "Download cancelled");
            DownloadEngine::global_cleanup();
            return 1;
        }
        std::this_thread::sleep_for(100ms);
    }

    DownloadEngine::global_cleanup();
    return 0;
}

CliResult info(const std::string& url) noexcept {
    DownloadEngine::global_init();

    core::HttpSession session;
    auto response = session.head(url);

    DownloadEngine::global_cleanup();

    if (!response) {
        compat::println(std::cerr, "Error: ", response.error().message());
        return std::unexpected(response.error());
    }

    compat::println("URL: ", url);
    compat::println("Status: ", response->status_code);
    compat::println("Content-Type: ", response->content_type);
    compat::println("Content-Length: ", compat::format(response->content_length));
    compat::println("Accepts-Ranges: ", response->accepts_ranges ? "yes" : "no");

    return 0;
}

void print_help(std::string_view program_name) noexcept {
    compat::println(R"(
    "Bolt Download Manager {} - High-speed download accelerator",
    program_name,
    "",
    "USAGE:",
    "  {} [OPTIONS] <URL>...",
    "",
    "OPTIONS:",
    "  -h, --help              Show this help message",
    "  -v, --version           Show version information",
    "  -V, --verbose           Enable verbose output",
    "  -q, --quiet             Quiet mode (no progress bar)",
    "  -o, --output <FILE>     Save to specified file",
    "  -d, --directory <DIR>   Save to specified directory",
    "  -n, --segments <N>      Number of segments (default: auto)",
    "  -i, --info              Show file info without downloading",
    "",
    "EXAMPLES:",
    "  {} https://example.com/file.zip",
    program_name,
    "  {} -o myfile.zip https://example.com/file.zip",
    program_name,
    "  {} -n 8 https://example.com/large.iso",
    program_name,
    "",
    "Created by changcheng967",
    "Copyright changcheng967 2026",
    "",
    "Built with C++23, Qt 6, libcurl",
    "Beats IDM in speed, UI, and architecture.");
}

void print_version() noexcept {
    compat::println("Bolt Download Manager ", bolt::version.to_string());
    compat::println("Created by changcheng967");
    compat::println("");
    compat::println("Built with C++23, Qt 6, libcurl");
    compat::println("Beats IDM in speed, UI, and architecture.");
}

} // namespace bolt::cli
