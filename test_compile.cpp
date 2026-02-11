// Copyright (c) 2026 changcheng967. All rights reserved.

#include <iostream>
#include <string>

// Minimal compile test without external dependencies
int main() {
    std::cout << "Bolt Download Manager - Compilation Test\n";
    std::cout << "Copyright (c) 2026 changcheng967. All rights reserved.\n\n";

    // Test std::string (basic C++ functionality)
    std::string test_str = "Hello, BoltDM!";
    std::cout << "String length: " << test_str.length() << "\n";
    std::cout << "Uppercase: ";
    for (char c : test_str) {
        std::cout << static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    std::cout << "\n\n";
    std::cout << "Build test passed!\n";
    std::cout << "The project structure is ready.\n";
    std::cout << "Install dependencies via vcpkg to build full project:\n";
    std::cout << "  vcpkg install curl boost-asio qt6 qt6-charts qt6-svg nlohmann-json spdlog openssl\n";

    return 0;
}
