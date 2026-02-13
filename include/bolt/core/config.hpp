// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <cstdint>
#include <chrono>

namespace bolt::core {

constexpr std::uint64_t DEFAULT_SEGMENT_SIZE = 5 * 1024 * 1024;      // 5 MB initial
constexpr std::uint64_t MIN_SEGMENT_SIZE = 256 * 1024;              // 256 KB (smaller for more parallelism)
constexpr std::uint64_t MAX_SEGMENT_SIZE = 50 * 1024 * 1024;        // 50 MB
constexpr std::uint32_t MAX_SEGMENTS = 32;                          // More segments for high bandwidth
constexpr std::uint32_t MIN_SEGMENTS = 4;                           // Start with at least 4

constexpr std::uint32_t CONNECTION_TIMEOUT_SEC = 30;
constexpr std::uint32_t IO_TIMEOUT_SEC = 60;
constexpr std::uint32_t STALL_TIMEOUT_SEC = 15;                     // Faster stall detection
constexpr std::uint32_t RETRY_COUNT = 3;

constexpr std::chrono::milliseconds BANDWIDTH_SAMPLE_INTERVAL{100};
constexpr std::chrono::milliseconds PROBE_DURATION{2000};

constexpr std::size_t WRITE_BUFFER_SIZE = 256 * 1024;           // 256 KB
constexpr std::size_t READ_BUFFER_SIZE = 256 * 1024;            // 256 KB

constexpr std::uint32_t MAX_REDIRECTS = 10;
constexpr bool FOLLOW_REDIRECTS = true;

} // namespace bolt::core
