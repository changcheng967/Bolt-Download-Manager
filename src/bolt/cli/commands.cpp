// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/cli/commands.hpp>
#include <bolt/cli/progress_bar.hpp>
#include <bolt/version.hpp>
#include <iostream>
#include <algorithm>

namespace bolt::cli {

namespace {

// Print verbose message
void vlog(const std::string& msg) noexcept {
    std::println(">>> {}", msg);
}

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

    if (verbose) vlog("Setting URL: " + url);

    auto result = engine->set_url(url);
    if (!result) {
        std::println(stderr, "Error: Invalid URL: {}", result.error().message());
        return std::unexpected(result.error());
    }

    if (!output.empty()) {
        engine->output_path(output);
        if (verbose) vlog("Output path: " + output);
    }

    // Configure
    DownloadConfig config;
    if (segments > 0) {
        config.max_segments = segments;
        config.min_segments = std::min(segments, 2u);
        config.auto_segment = false;
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
                vlog(std::format("Active: {}, Completed: {}, Speed: {}",
                    progress.active_segments,
                    progress.completed_segments,
                    ProgressBar::format_speed(progress.speed_bps)));
            }
        });
    }

    if (verbose) vlog("Starting download...");
    auto start_result = engine->start();
    if (start_result) {
        std::println(stderr, "Error: Failed to start download: {}", start_result.message());
        DownloadEngine::global_cleanup();
        return std::unexpected(start_result);
    }

    // Wait for completion (polling state)
    using namespace std::chrono_literals;
    while (true) {
        auto state = engine->state();
        if (state == DownloadState::completed) {
            if (!quiet) bar.finish();
            if (verbose) vlog("Download completed!");
            break;
        }
        if (state == DownloadState::failed) {
            if (!quiet) bar.clear();
            std::println(stderr, "Error: Download failed");
            DownloadEngine::global_cleanup();
            return std::unexpected(make_error_code(DownloadErrc::network_error));
        }
        if (state == DownloadState::cancelled) {
            if (!quiet) bar.clear();
            std::println(stderr, "Download cancelled");
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

    HttpSession session;
    auto response = session.head(url);

    DownloadEngine::global_cleanup();

    if (!response) {
        std::println(stderr, "Error: {}", response.error().message());
        return std::unexpected(response.error());
    }

    std::println("URL: {}", url);
    std::println("Status: {}", response->status_code);
    std::println("Content-Type: {}", response->content_type);
    std::println("Content-Length: {}", ProgressBar::format_bytes(response->content_length));
    std::println("Accepts-Ranges: {}", response->accepts_ranges ? "yes" : "no");

    return 0;
}

void print_help(std::string_view program_name) noexcept {
    std::println(R"(
Bolt Download Manager {} - High-speed download accelerator

USAGE:
    {} [OPTIONS] <URL>...

OPTIONS:
    -h, --help              Show this help message
    -v, --version           Show version information
    -V, --verbose           Enable verbose output
    -q, --quiet             Quiet mode (no progress bar)
    -o, --output <FILE>     Save to specified file
    -d, --directory <DIR>   Save to specified directory
    -n, --segments <N>      Number of segments (default: auto)
    -i, --info              Show file info without downloading

EXAMPLES:
    {} https://example.com/file.zip
    {} -o myfile.zip https://example.com/file.zip
    {} -n 8 https://example.com/large.iso

Created by changcheng967
© changcheng967 2026
)",
        bolt::version.to_string(),
        program_name,
        program_name, program_name, program_name);
}

void print_version() noexcept {
    std::println("Bolt Download Manager {}", bolt::version.to_string());
    std::println("Created by changcheng967");
    std::println("© changcheng967 2026");
    std::println("");
    std::println("Built with C++23, Qt 6, libcurl");
    std::println("Beats IDM in speed, UI, and architecture.");
}

} // namespace bolt::cli
