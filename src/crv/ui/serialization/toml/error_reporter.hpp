// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/ui/serialization/exceptions.hpp>
#include <sstream>
#include <string_view>
#include <toml++/toml.hpp>

namespace crv::serialization::tomlpp {

/// error reporting for a toml node
class error_reporter_t
{
public:
    /// records most recently visited source region
    auto location(toml::source_region const& src) -> void { location_ = src; }

    /// throws parse_x using the recorded location with given message
    [[noreturn]] auto report_error(std::string_view message) const -> void
    {
        auto out = std::ostringstream{};
        out << *location_.path << ":" << location_.begin.line << ":" << location_.begin.column << ": " << message;
        throw parse_x{out.str()};
    }

private:
    toml::source_region location_{};
};

} // namespace crv::serialization::tomlpp
