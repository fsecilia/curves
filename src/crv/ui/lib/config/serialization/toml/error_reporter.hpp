// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <string_view>
#include <toml++/toml.hpp>

namespace crv::config::tomlpp {

/// error reporting for a toml node
class error_reporter_t
{
public:
    /// records most recently visited source region
    auto location(toml::source_region const& src) -> void { location_ = src; }

    /// throws a parse error using the recorded location with given message
    [[noreturn]] auto report_error(std::string_view message) const -> void
    {
        throw toml::parse_error{message.data(), location_};
    }

private:
    toml::source_region location_{};
};

} // namespace crv::config::tomlpp
