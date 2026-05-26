// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "sequential_enum_name_map.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

enum class enum_t
{
    value_0,
    value_1,
    value_2,
};

// zero
constexpr auto sut_size_0 = sequential_enum_name_map<enum_t>();
static_assert(std::array<std::string_view, 0>{} == sut_size_0);
static_assert(std::nullopt == sut_size_0.to_string(enum_t::value_0));
static_assert(std::nullopt == sut_size_0.from_string("value_0"));

// one
constexpr auto sut_size_1 = sequential_enum_name_map<enum_t>("value_0");
static_assert(std::array<std::string_view, 1>{"value_0"} == sut_size_1);
static_assert("value_0" == sut_size_1.to_string(enum_t::value_0));
static_assert(std::nullopt == sut_size_1.to_string(enum_t::value_1));
static_assert(enum_t::value_0 == sut_size_1.from_string("value_0"));
static_assert(std::nullopt == sut_size_0.from_string("value_0"));

// many
constexpr auto sut_size_3 = sequential_enum_name_map<enum_t>("value_0", "value_1", "value_2");
static_assert(std::array<std::string_view, 3>{"value_0", "value_1", "value_2"} == sut_size_3);
static_assert("value_0" == sut_size_3.to_string(enum_t::value_0));
static_assert("value_1" == sut_size_3.to_string(enum_t::value_1));
static_assert("value_2" == sut_size_3.to_string(enum_t::value_2));
static_assert(enum_t::value_0 == sut_size_3.from_string("value_0"));
static_assert(enum_t::value_1 == sut_size_3.from_string("value_1"));
static_assert(enum_t::value_2 == sut_size_3.from_string("value_2"));

} // namespace
} // namespace crv
