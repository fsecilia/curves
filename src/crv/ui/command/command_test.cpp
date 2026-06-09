// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "command.hpp"
#include <crv/test/test.hpp>
#include <concepts>

namespace crv::command {
namespace {

using sut_t = factory_t;
sut_t sut;

struct empty_command_t
{};
static_assert(std::same_as<empty_command_t, decltype(sut.template create<empty_command_t>())>);

struct single_param_t
{
    int_t param = 0;

    constexpr auto operator==(single_param_t const&) const noexcept -> bool = default;
};
static_assert(single_param_t{3} == sut.template create<single_param_t>(3));

struct multiple_params_t
{
    int_t param0 = 0;
    bool param1 = false;
    float_t param2 = 0.0;

    constexpr auto operator==(multiple_params_t const&) const noexcept -> bool = default;
};
static_assert(multiple_params_t{3, true, 8.0} == sut.template create<multiple_params_t>(3, true, 8.0));

} // namespace
} // namespace crv::command
