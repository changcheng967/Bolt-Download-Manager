// Copyright (c) 2026 changcheng967. All rights reserved.

#include <catch2/catch_all.hpp>
#include <bolt/core/download_meta.hpp>
#include <filesystem>
#include <fstream>

using namespace bolt::core;

namespace fs = std::filesystem;

TEST_CASE("DownloadMeta::meta_path", "[metadata]") {
    SECTION("Appends .boltmeta extension") {
        CHECK(DownloadMeta::meta_path("test.bin") == "test.bin.boltmeta");
        CHECK(DownloadMeta::meta_path("/path/to/file.zip") == "/path/to/file.zip.boltmeta");
    }

    SECTION("Handles paths with spaces") {
        CHECK(DownloadMeta::meta_path("/path/with spaces/file.bin") == "/path/with spaces/file.bin.boltmeta");
    }
}

TEST_CASE("DownloadMeta save and load", "[metadata]") {
    const std::string test_file = "test_meta.bin";
    const std::string meta_file = test_file + ".boltmeta";

    // Cleanup any existing files
    if (fs::exists(meta_file)) fs::remove(meta_file);

    SECTION("Save and load round-trip") {
        DownloadMeta original;
        original.url = "https://example.com/large_file.zip";
        original.output_path = test_file;
        original.file_size = 100'000'000;
        original.total_downloaded = 45'000'000;

        // Add segment metadata
        SegmentMeta seg1{0, 0, 25'000'000, 0, 12'000'000};
        SegmentMeta seg2{1, 25'000'000, 25'000'000, 25'000'000, 11'000'000};
        SegmentMeta seg3{2, 50'000'000, 25'000'000, 50'000'000, 11'000'000};
        SegmentMeta seg4{3, 75'000'000, 25'000'000, 75'000'000, 11'000'000};
        original.segments = {seg1, seg2, seg3, seg4};

        // Save
        auto save_result = original.save(meta_file);
        REQUIRE_FALSE(save_result);
        REQUIRE(fs::exists(meta_file));

        // Load
        auto load_result = DownloadMeta::load(meta_file);
        REQUIRE(load_result.has_value());

        const auto& loaded = *load_result;
        CHECK(loaded.url == original.url);
        CHECK(loaded.output_path == original.output_path);
        CHECK(loaded.file_size == original.file_size);
        CHECK(loaded.total_downloaded == original.total_downloaded);
        CHECK(loaded.segments.size() == original.segments.size());

        for (size_t i = 0; i < original.segments.size(); ++i) {
            CHECK(loaded.segments[i].id == original.segments[i].id);
            CHECK(loaded.segments[i].offset == original.segments[i].offset);
            CHECK(loaded.segments[i].size == original.segments[i].size);
            CHECK(loaded.segments[i].file_offset == original.segments[i].file_offset);
            CHECK(loaded.segments[i].downloaded == original.segments[i].downloaded);
        }
    }

    SECTION("Load non-existent file returns error") {
        auto result = DownloadMeta::load("non_existent_file.boltmeta");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Save creates parent directories if needed") {
        const std::string nested_path = "test_dir/nested/meta.bin.boltmeta";

        // Clean up
        if (fs::exists("test_dir")) fs::remove_all("test_dir");

        DownloadMeta meta;
        meta.url = "https://example.com/file.zip";
        meta.output_path = "test_dir/nested/meta.bin";
        meta.file_size = 1000;
        meta.total_downloaded = 500;

        auto save_result = meta.save(nested_path);
        REQUIRE_FALSE(save_result);
        REQUIRE(fs::exists(nested_path));

        // Cleanup
        fs::remove_all("test_dir");
    }

    // Cleanup
    if (fs::exists(meta_file)) fs::remove(meta_file);
}

TEST_CASE("DownloadMeta exists and remove", "[metadata]") {
    const std::string test_file = "test_exists.bin";
    const std::string meta_file = test_file + ".boltmeta";

    // Clean up first
    if (fs::exists(meta_file)) fs::remove(meta_file);

    SECTION("exists returns false when file doesn't exist") {
        CHECK_FALSE(DownloadMeta::exists(test_file));
    }

    SECTION("exists returns true when file exists") {
        // Create the meta file
        std::ofstream(meta_file).close();
        CHECK(DownloadMeta::exists(test_file));
        fs::remove(meta_file);
    }

    SECTION("remove deletes the meta file") {
        // Create the meta file
        std::ofstream(meta_file).close();
        REQUIRE(fs::exists(meta_file));

        DownloadMeta::remove(test_file);
        CHECK_FALSE(fs::exists(meta_file));
    }

    SECTION("remove is safe when file doesn't exist") {
        // Should not throw
        REQUIRE_NOTHROW(DownloadMeta::remove("non_existent_file.bin"));
    }
}

TEST_CASE("SegmentMeta defaults", "[metadata]") {
    SegmentMeta seg;
    CHECK(seg.id == 0);
    CHECK(seg.offset == 0);
    CHECK(seg.size == 0);
    CHECK(seg.file_offset == 0);
    CHECK(seg.downloaded == 0);
}

TEST_CASE("DownloadMeta defaults", "[metadata]") {
    DownloadMeta meta;
    CHECK(meta.url.empty());
    CHECK(meta.output_path.empty());
    CHECK(meta.file_size == 0);
    CHECK(meta.total_downloaded == 0);
    CHECK(meta.segments.empty());
}
