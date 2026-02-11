// Copyright (c) 2026 changcheng967. All rights reserved.

#include <catch2/catch_all.hpp>
#include <bolt/core/bandwidth_prober.hpp>
#include <bolt/core/config.hpp>

using namespace bolt::core;

TEST_CASE("SegmentCalculator::optimal_segments", "[bandwidth]") {
    SegmentCalculator calc(100'000'000);  // 100 MB file

    SECTION("High bandwidth - fewer segments") {
        std::uint64_t high_bandwidth = 200'000'000;  // 200 MB/s
        std::uint32_t segs = calc.optimal_segments(high_bandwidth);
        CHECK(segs == MIN_SEGMENTS);
    }

    SECTION("Low bandwidth - more segments") {
        std::uint64_t low_bandwidth = 500'000;  // 500 KB/s
        std::uint32_t segs = calc.optimal_segments(low_bandwidth);
        CHECK(segs == MAX_SEGMENTS);
    }

    SECTION("Medium bandwidth - balanced") {
        std::uint64_t med_bandwidth = 10'000'000;  // 10 MB/s
        std::uint32_t segs = calc.optimal_segments(med_bandwidth);
        CHECK(segs >= MIN_SEGMENTS);
        CHECK(segs <= MAX_SEGMENTS);
    }
}

TEST_CASE("SegmentCalculator::optimal_segment_size", "[bandwidth]") {
    std::uint64_t file_size = 50'000'000;  // 50 MB

    SECTION("Few segments - larger size") {
        SegmentCalculator calc(file_size);
        std::uint32_t few_segments = 4;
        std::uint64_t seg_size = calc.optimal_segment_size(few_segments);
        CHECK(seg_size == 12'500'000);  // 50MB / 4
    }

    SECTION("Clamp to max segment size") {
        SegmentCalculator calc(500'000'000);  // 500 MB file
        std::uint32_t many_segments = 4;
        std::uint64_t seg_size = calc.optimal_segment_size(many_segments);

        // 500MB / 4 = 125MB, should be clamped to MAX_SEGMENT_SIZE
        CHECK(seg_size <= MAX_SEGMENT_SIZE);
    }

    SECTION("Clamp to min segment size") {
        SegmentCalculator calc(1'000'000);  // 1 MB file
        std::uint32_t many_segments = 16;
        std::uint64_t seg_size = calc.optimal_segment_size(many_segments);

        // 1MB / 16 = 64KB, should be clamped to MIN_SEGMENT_SIZE
        CHECK(seg_size >= MIN_SEGMENT_SIZE);
    }
}

TEST_CASE("SegmentCalculator::use_work_stealing", "[bandwidth]") {
    SegmentCalculator calc;

    SECTION("Significant speed variance") {
        std::uint64_t avg = 1'000'000;
        std::uint64_t fast = 2'000'000;
        std::uint64_t slow = 500'000;

        bool should_steal = calc.use_work_stealing(avg, fast, slow);
        CHECK(should_steal == true);
    }

    SECTION("Small speed variance") {
        std::uint64_t avg = 1'000'000;
        std::uint64_t fast = 1'200'000;
        std::uint64_t slow = 900'000;

        bool should_steal = calc.use_work_stealing(avg, fast, slow);
        CHECK(should_steal == false);
    }

    SECTION("Zero speed segment") {
        std::uint64_t avg = 500'000;
        std::uint64_t fast = 1'000'000;
        std::uint64_t slow = 0;

        bool should_steal = calc.use_work_stealing(avg, fast, slow);
        CHECK(should_steal == true);
    }
}

TEST_CASE("Speed formatting", "[bandwidth]") {
    SECTION("Bytes per second") {
        std::uint64_t bps = 512;
        std::string formatted = std::format("{}/s", bps);
        CHECK(formatted == "512/s");
    }

    SECTION("KB per second") {
        std::uint64_t bps = 512 * 1024;
        std::string formatted = std::format("{:.1f} KB/s", bps / 1024.0);
        CHECK(formatted == "512.0 KB/s");
    }

    SECTION("MB per second") {
        std::uint64_t bps = 10 * 1024 * 1024;
        std::string formatted = std::format("{:.1f} MB/s", bps / (1024.0 * 1024.0));
        CHECK(formatted == "10.0 MB/s");
    }

    SECTION("GB per second") {
        std::uint64_t bps = 5ULL * 1024 * 1024 * 1024;
        std::string formatted = std::format("{:.2f} GB/s", bps / (1024.0 * 1024 * 1024));
        CHECK(formatted == "5.00 GB/s");
    }
}

TEST_CASE("ETA calculation", "[bandwidth]") {
    SECTION("Zero speed") {
        std::uint64_t remaining = 10'000'000;
        std::uint64_t speed = 0;
        std::uint64_t eta = (speed > 0) ? remaining / speed : 0;
        CHECK(eta == 0);
    }

    SECTION("Simple ETA") {
        std::uint64_t remaining = 60'000'000;  // 60 MB
        std::uint64_t speed = 1'000'000;  // 1 MB/s
        std::uint64_t eta = remaining / speed;
        CHECK(eta == 60);  // 60 seconds
    }

    SECTION("Large download ETA") {
        std::uint64_t remaining = 5'000'000'000;  // 5 GB
        std::uint64_t speed = 25'000'000;  // 25 MB/s
        std::uint64_t eta = remaining / speed;
        CHECK(eta == 200);  // ~3.3 minutes
    }
}
