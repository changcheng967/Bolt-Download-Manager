// Copyright (c) 2026 changcheng967. All rights reserved.

#include <catch2/catch_all.hpp>
#include <bolt/core/url.hpp>

using namespace bolt::core;

TEST_CASE("Url::parse - valid URLs", "[url]") {
    SECTION("HTTPS URL") {
        auto result = Url::parse("https://example.com/file.zip");
        REQUIRE(result.has_value());
        auto url = *result;
        CHECK(url.scheme() == "https");
        CHECK(url.host() == "example.com");
        CHECK(url.path() == "/file.zip");
        CHECK(url.is_secure());
    }

    SECTION("HTTP URL with port") {
        auto result = Url::parse("http://example.com:8080/path");
        REQUIRE(result.has_value());
        auto url = *result;
        CHECK(url.scheme() == "http");
        CHECK(url.port() == "8080");
        CHECK(!url.is_secure());
    }

    SECTION("URL with query and fragment") {
        auto result = Url::parse("https://example.com/file.zip?v=1#section");
        REQUIRE(result.has_value());
        auto url = *result;
        CHECK(url.query() == "v=1");
        CHECK(url.fragment() == "section");
    }

    SECTION("URL with path segments") {
        auto result = Url::parse("https://cdn.example.com/downloads/v1.2/files/archive.zip");
        REQUIRE(result.has_value());
        auto url = *result;
        CHECK(url.path() == "/downloads/v1.2/files/archive.zip");
    }
}

TEST_CASE("Url::parse - invalid URLs", "[url]") {
    SECTION("Missing scheme") {
        auto result = Url::parse("example.com/file.zip");
        REQUIRE(!result.has_value());
    }

    SECTION("Empty string") {
        auto result = Url::parse("");
        REQUIRE(!result.has_value());
    }
}

TEST_CASE("Url::filename extraction", "[url]") {
    SECTION("Simple filename") {
        auto result = Url::parse("https://example.com/myfile.zip");
        REQUIRE(result.has_value());
        CHECK(result->filename() == "myfile.zip");
    }

    SECTION("URL with query params") {
        auto result = Url::parse("https://example.com/download.php?id=123");
        REQUIRE(result.has_value());
        CHECK(result->filename() == "download.php");
    }

    SECTION("Path without filename") {
        auto result = Url::parse("https://example.com/folder/");
        REQUIRE(result.has_value());
        CHECK(result->filename() == "index.html");
    }
}

TEST_CASE("Url::default_port", "[url]") {
    CHECK(Url::parse("https://example.com")->default_port() == 443);
    CHECK(Url::parse("http://example.com")->default_port() == 80);
    CHECK(Url::parse("ftp://example.com")->default_port() == 21);
}
