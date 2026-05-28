// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "error_reporting.hpp"
#include <crv/serialization/exceptions.hpp>

namespace crv::serialization::tomlpp {

[[noreturn]] auto report_error(std::string const& message, toml::source_region location) -> void
{
    if (location.path)
    {
        report_error(std::format("{}:{}:{}: {}", *location.path, location.begin.line, location.begin.column, message));
    }
    else
    {
        report_error(std::format("{}:{}: {}", location.begin.line, location.begin.column, message));
    }
}

[[noreturn]] auto report_error(std::string message) -> void
{
    throw parse_x{std::move(message)};
}

} // namespace crv::serialization::tomlpp
