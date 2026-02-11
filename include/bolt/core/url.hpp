// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/core/error.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <expected>

namespace bolt::core {

class Url {
public:
    static std::expected<Url, std::error_code> parse(std::string_view url_str) noexcept;

    [[nodiscard]] std::string_view scheme() const noexcept { return scheme_; }
    [[nodiscard]] std::string_view host() const noexcept { return host_; }
    [[nodiscard]] std::string_view port() const noexcept { return port_; }
    [[nodiscard]] std::string_view path() const noexcept { return path_; }
    [[nodiscard]] std::string_view query() const noexcept { return query_; }
    [[nodiscard]] std::string_view fragment() const noexcept { return fragment_; }

    [[nodiscard]] std::string full() const;
    [[nodiscard]] std::string base() const;  // scheme://host[:port]
    [[nodiscard]] bool is_secure() const noexcept { return scheme_ == "https"; }

    [[nodiscard]] std::uint16_t default_port() const noexcept;
    [[nodiscard]] std::string filename() const;

    Url() = default;
    explicit Url(std::string url_str) : str_(std::move(url_str)) {}

private:
    std::string str_;
    std::string_view scheme_;
    std::string_view host_;
    std::string_view port_;
    std::string_view path_;
    std::string_view query_;
    std::string_view fragment_;
};

} // namespace bolt::core
