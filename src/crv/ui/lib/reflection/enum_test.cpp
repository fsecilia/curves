// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "enum.hpp"
#include <crv/test/test.hpp>

namespace crv::reflection {
namespace {

enum class mapped_enum_t
{
    value_0,
    value_1,
    value_2,
};

} // namespace

template <> struct enum_t<mapped_enum_t>
{
    static constexpr auto map = sequential_enum_name_map<mapped_enum_t>("value_0", "value_1", "value_2");
};

static_assert(std::array<std::string_view, 3>{"value_0", "value_1", "value_2"} == enum_t<mapped_enum_t>::map);
static_assert("value_0" == to_string(mapped_enum_t::value_0));
static_assert("value_1" == to_string(mapped_enum_t::value_1));
static_assert("value_2" == to_string(mapped_enum_t::value_2));
static_assert(mapped_enum_t::value_0 == from_string<mapped_enum_t>("value_0"));
static_assert(mapped_enum_t::value_1 == from_string<mapped_enum_t>("value_1"));
static_assert(mapped_enum_t::value_2 == from_string<mapped_enum_t>("value_2"));
static_assert(std::nullopt == from_string<mapped_enum_t>("value_3"));

} // namespace crv::reflection
