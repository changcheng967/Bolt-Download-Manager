// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/core/error.hpp>
#include <string>
#include <utility>
#include <expected>

namespace bolt::core {

class Url {
public:
    static std::expected<Url, std::error_code> parse(std::string_view url_str) noexcept;

    [[nodiscard]] const std::string& scheme() const noexcept { return scheme_; }
    [[nodiscard]] const std::string& host() const noexcept { return host_; }
    [[nodiscard]] const std::string& port() const noexcept { return port_; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    [[nodiscard]] const std::string& query() const noexcept { return query_; }
    [[nodiscard]] const std::string& fragment() const noexcept { return fragment_; }

    [[nodiscard]] std::string full() const;
    [[nodiscard]] std::string base() const;  // scheme://host[:port]
    [[nodiscard]] bool is_secure() const noexcept { return scheme_ == "https"; }

    [[nodiscard]] std::uint16_t default_port() const noexcept;
    [[nodiscard]] std::string filename() const;

    Url() = default;
    explicit Url(std::string url_str) : str_(std::move(url_str)) {}

private:
    std::string str_;
    std::string scheme_;
    std::string host_;
    std::string port_;
    std::string path_;
    std::string query_;
    std::string fragment_;
};

} // namespace bolt::core
