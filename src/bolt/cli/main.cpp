// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/cli/commands.hpp>
#include <bolt/version.hpp>
#include <iostream>

using namespace bolt::cli;

int main(int argc, char* argv[]) {
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
