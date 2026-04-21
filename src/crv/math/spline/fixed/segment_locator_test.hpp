// SPDX-License-Identifier: MIT

/// \file
/// \brief builder for spline segment locator
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/spline/fixed/segment_locator.hpp>
#include <crv/test/test.hpp>

namespace crv::spline::fixed_point::segment_locator::test {

template <typename location_t, int_t count> constexpr auto make_sequential_keys() -> std::array<location_t, count>
{
    std::array<location_t, count> result;
    for (auto i = 0; i < count; ++i) result[i] = location_t{i + 1};
    return result;
}

template <typename location_t, int_t count>
constexpr auto make_strided_keys(int_t offset, int_t stride) -> std::array<location_t, count>
{
    std::array<location_t, count> result;
    for (auto i = 0; i < count; ++i) result[i] = location_t{offset + i * stride};
    return result;
}

} // namespace crv::spline::fixed_point::segment_locator::test
