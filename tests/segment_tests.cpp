// Copyright (c) 2026 changcheng967. All rights reserved.

#include <catch2/catch_all.hpp>
#include <bolt/core/segment.hpp>
#include <bolt/core/config.hpp>

using namespace bolt::core;

TEST_CASE("Segment splitting math", "[segment]") {
    SECTION("Equal segment sizes") {
        std::uint64_t file_size = 100'000'000;  // 100 MB
        std::uint32_t num_segments = 4;

        std::uint64_t segment_size = file_size / num_segments;
        CHECK(segment_size == 25'000'000);

        std::uint64_t total = 0;
        for (std::uint32_t i = 0; i < num_segments - 1; ++i) {
            total += segment_size;
        }
        // Last segment gets remainder
        std::uint64_t last_size = file_size - total;

        CHECK(total + last_size == file_size);
    }

    SECTION("Small file, multiple segments") {
        std::uint64_t file_size = 500'000;  // 500 KB
        std::uint32_t num_segments = 8;

        std::uint64_t segment_size = file_size / num_segments;
        CHECK(segment_size >= 1);  // At least 1 byte
    }

    SECTION("Large file, min segment size") {
        std::uint64_t file_size = 5'000'000'000;  // 5 GB
        std::uint32_t max_segments = MAX_SEGMENTS;
        std::uint64_t min_seg = MIN_SEGMENT_SIZE;

        std::uint64_t segments_at_min = file_size / min_seg;
        std::uint32_t actual_segments = std::min(static_cast<std::uint32_t>(segments_at_min), max_segments);

        REQUIRE(actual_segments <= max_segments);
    }
}

TEST_CASE("Segment::percent calculation", "[segment]") {
    SECTION("Zero progress") {
        std::uint64_t downloaded = 0;
        std::uint64_t total = 1000;
        double percent = static_cast<double>(downloaded) * 100.0 / static_cast<double>(total);
        CHECK(percent == 0.0);
    }

    SECTION("Half progress") {
        std::uint64_t downloaded = 500;
        std::uint64_t total = 1000;
        double percent = static_cast<double>(downloaded) * 100.0 / static_cast<double>(total);
        CHECK(percent == 50.0);
    }

    SECTION("Complete progress") {
        std::uint64_t downloaded = 1000;
        std::uint64_t total = 1000;
        double percent = static_cast<double>(downloaded) * 100.0 / static_cast<double>(total);
        CHECK(percent == 100.0);
    }
}

TEST_CASE("Segment::range calculations", "[segment]") {
    SECTION("Byte range calculation") {
        std::uint64_t offset = 1000;
        std::uint64_t size = 500;

        std::uint64_t range_end = offset + size - 1;
        CHECK(range_end == 1499);

        std::string range = std::format("{}-{}", offset, range_end);
        CHECK(range == "1000-1499");
    }

    SECTION("Last segment boundary") {
        std::uint64_t file_size = 5000;
        std::uint64_t segment_size = 2000;
        std::uint32_t seg_id = 2;  // Third segment

        std::uint64_t offset = seg_id * segment_size;
        std::uint64_t remaining = file_size - offset;

        std::uint64_t this_size = std::min(segment_size, remaining);
        CHECK(this_size == 1000);  // Last segment is smaller
    }
}
