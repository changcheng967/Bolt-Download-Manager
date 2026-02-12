// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/cli/commands.hpp>
#include <bolt/cli/progress_bar.hpp>
#include <bolt/core/download_engine.hpp>
#include <bolt/core/error.hpp>
#include <bolt/core/http_session.hpp>
#include <bolt/version.hpp>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <expected>
#include <chrono>
#include <thread>
#include <sstream>

using namespace bolt::cli;
using namespace bolt::core;

namespace chrono = std::chrono;

namespace bolt::cli {

const char* SPINNER_FRAMES[] = {"-", "\\", "|", "/"};

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
        }
        if (arg == "-q" || arg == "--quiet") {
            args.quiet = true;
        }
        if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                args.output_file = argv[++i];
            }
        }
        if (arg == "-d" || arg == "--directory") {
            if (i + 1 < argc) {
                args.output_dir = argv[++i];
            }
        }
        if (arg == "-n" || arg == "--segments") {
            if (i + 1 < argc) {
                char* end = nullptr;
                args.segments = std::strtoul(argv[++i], &end, 10);
                if (end == nullptr || *end != '\0') {
                    args.segments = 0;
                }
            }
        }
        if (arg == "-i" || arg == "--info") {
            args.list_only = true;
        }
        // URL arguments (no option)
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

    if (verbose) {
        std::cout << "Setting URL: " << url << std::endl;
    }

    auto result = engine->set_url(url);
    if (!result) {
        std::cout << "Error: Invalid URL: " << result.error() << std::endl;
        return std::unexpected(result.error());
    }

    if (!output.empty()) {
        engine->output_path(output);
        if (verbose) {
            std::cout << "Output path: " << output << std::endl;
        }
    }

    // Configure
    DownloadConfig config;
    if (segments > 0) {
        config.max_segments = segments;
        config.min_segments = std::min(segments, 2u);
    } else {
        config.max_segments = 2;
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
                std::cout << "Active: " << progress.active_segments
                         << " Completed: " << progress.completed_segments
                         << " Speed: " << progress.speed_bps << std::endl;
            }
        });
    }

    if (verbose) {
        std::cout << "Starting download..." << std::endl;
    }

    auto start_result = engine->start();
    if (start_result) {
        std::cout << "Error: Failed to start download: " << start_result << std::endl;
        DownloadEngine::global_cleanup();
        return std::unexpected(start_result);
    }

    // Wait for completion
    while (true) {
        auto state = engine->state();
        if (state == DownloadState::completed) {
            if (!quiet) bar.finish();
            if (verbose) {
                std::cout << "Download completed!" << std::endl;
            }
            break;
        }
        if (state == DownloadState::failed) {
            if (!quiet) bar.clear();
            std::cout << "Error: Download failed" << std::endl;
            DownloadEngine::global_cleanup();
            return std::unexpected(make_error_code(DownloadErrc::network_error));
        }
        if (state == DownloadState::cancelled) {
            if (!quiet) bar.clear();
            std::cout << "Download cancelled" << std::endl;
            DownloadEngine::global_cleanup();
            return 1;
        }
        std::this_thread::sleep_for(chrono::milliseconds(100));
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
        std::cout << "Error: " << response.error() << std::endl;
        return std::unexpected(response.error());
    }

    std::cout << "URL: " << url << std::endl;
    std::cout << "Status: " << response->status_code << std::endl;
    std::cout << "Content-Type: " << response->content_type << std::endl;
    std::cout << "Content-Length: " << response->content_length << std::endl;
    std::cout << "Accepts-Ranges: " << (response->accepts_ranges ? "yes" : "no") << std::endl;

    return 0;
}

void print_help(std::string_view program_name) noexcept {
    std::cout << "Bolt Download Manager " << program_name << " - High-speed download accelerator\n";
    std::cout << "\n";
    std::cout << "USAGE:\n";
    std::cout << "  " << program_name << " [OPTIONS] <URL>...\n";
    std::cout << "\n";
    std::cout << "OPTIONS:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -v, --version           Show version information\n";
    std::cout << "  -V, --verbose           Enable verbose output\n";
    std::cout << "  -q, --quiet             Quiet mode (no progress bar)\n";
    std::cout << "  -o, --output <FILE>     Save to specified file\n";
    std::cout << "  -d, --directory <DIR>   Save to specified directory\n";
    std::cout << "  -n, --segments <N>      Number of segments (default: auto)\n";
    std::cout << "  -i, --info              Show file info without downloading\n";
    std::cout << "\n";
    std::cout << "EXAMPLES:\n";
    std::cout << "  " << program_name << " https://example.com/file.zip\n";
    std::cout << "  " << program_name << " -o myfile.zip https://example.com/file.zip\n";
    std::cout << "  " << program_name << " -n 8 https://example.com/large.iso\n";
    std::cout << "\n";
    std::cout << "Created by changcheng967\n";
    std::cout << "Copyright changcheng967 2026\n";
}

void print_version() noexcept {
    std::cout << "Bolt Download Manager " << bolt::version.to_string() << std::endl;
    std::cout << "Created by changcheng967\n";
    std::cout << "\n";
    std::cout << "Built with C++23, Qt 6, libcurl\n";
    std::cout << "Beats IDM in speed, UI, and architecture.\n";
}

} // namespace bolt::cli
