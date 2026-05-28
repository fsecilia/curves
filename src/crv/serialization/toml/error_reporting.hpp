// SPDX-License-Identifier: MIT

/// \file
/// \brief toml error reporting
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <string>
#include <toml++/toml.hpp>

namespace crv::serialization::tomlpp {

/// throws parse_x with formatted location and message
[[noreturn]] auto report_error(std::string const& message, toml::source_region location) -> void;

/// throws parse_x with basic message
[[noreturn]] auto report_error(std::string message) -> void;

} // namespace crv::serialization::tomlpp
