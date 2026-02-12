// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/cli/commands.hpp>
#include <bolt/version.hpp>
#include <iostream>
#include <exception>
#include <cstdlib>

using namespace bolt::cli;

// Terminate handler to catch exceptions in noexcept functions
static void bolt_terminate_handler() {
    static bool in_terminate = false;
    if (in_terminate) {
        std::abort();  // Prevent re-entrant abort
    }
    in_terminate = true;

    std::cerr << "FATAL: std::terminate called!" << std::endl;
    try {
        std::rethrow_exception(std::current_exception());
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in noexcept context" << std::endl;
    }
    std::cerr << "Aborting..." << std::endl;
    std::abort();
}

int main(int argc, char* argv[]) {
    std::set_terminate(bolt_terminate_handler);
    // Parse arguments
    CliArgs args = parse_args(argc, argv);

    // Handle help
    if (args.help) {
        print_help(argv[0]);
        return 0;
    }

    // Handle version
    if (args.version) {
        print_version();
        return 0;
    }

    // Need at least one URL
    if (args.urls.empty()) {
        std::cerr << "Error: No URL specified" << std::endl;
        std::cout << "Use -h for help" << std::endl;
        return 1;
    }

    // Handle info mode
    if (args.list_only) {
        for (const auto& url : args.urls) {
            auto result = info(url);
            if (!result) {
                return 1;
            }
        }
        return 0;
    }

    // Download mode
    int exit_code = 0;
    for (const auto& url : args.urls) {
        std::string output = args.output_file;
        if (args.urls.size() > 1 && output.empty()) {
            // Multiple URLs: generate filename from URL
            auto parsed = bolt::core::Url::parse(url);
            if (parsed) {
                output = std::string(parsed->filename());
            }
        }

        auto result = download(url, output, args.segments, args.verbose, args.quiet);
        if (!result) {
            exit_code = 1;
        }
    }

    return exit_code;
}
