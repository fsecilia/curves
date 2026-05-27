// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "concepts.hpp"
#include <crv/test/test.hpp>
#include <string>

namespace crv {
namespace serialization::tomlpp {
namespace {

static_assert(is_toml_primitive<int_t>);
static_assert(is_toml_primitive<float_t>);
static_assert(is_toml_primitive<char const*>);
static_assert(is_toml_primitive<std::string>);
static_assert(is_toml_primitive<std::string_view>);

enum class enum_t
{
};
static_assert(is_toml_primitive<enum_t>);

} // namespace
} // namespace serialization::tomlpp
} // namespace crv
