// SPDX-License-Identifier: MIT

/// \file
/// \brief validates an encoded runtime spline before it becomes trusted
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <type_traits>

namespace crv::spline {

enum class spline_validation_error_t : uint8_t
{
    none,
    locator,
    segment,
    tangent,
    tangent_anchor,
};

struct spline_validation_result_t
{
    spline_validation_error_t error{};
    int_t segment_index = -1;

    constexpr explicit operator bool() const noexcept { return error == spline_validation_error_t::none; }
    constexpr auto operator==(spline_validation_result_t const&) const noexcept -> bool = default;
};

/// validates the complete encoded spline representation used by runtime evaluation
template <typename t_spline_t> struct spline_validator_t
{
    using spline_t = t_spline_t;
    using x_t = spline_t::x_t;

    CRV_ALWAYS_INLINE
    constexpr auto operator()(spline_t const& spline) const noexcept -> spline_validation_result_t
    {
        static_assert(std::is_trivially_copyable_v<spline_t>);
        static_assert(std::is_standard_layout_v<spline_t>);

        auto const& locator = spline.segment_locator;
        if (!locator.is_valid()) return {.error = spline_validation_error_t::locator};

        auto const segment_count = locator.segment_count();
        for (auto segment_index = int_t{0}; segment_index < segment_count; ++segment_index)
        {
            auto const origin = locator.segment_origin(segment_index);
            auto const end = locator.segment_end(segment_index);
            auto const width = x_t::literal(end.value - origin.value);
            if (!spline.segments[segment_index].is_safe_through(width, origin))
            {
                return {.error = spline_validation_error_t::segment, .segment_index = segment_index};
            }
        }

        if (!spline.extend_final_tangent.is_safe()) return {.error = spline_validation_error_t::tangent};

        auto const final_segment_index = segment_count - 1;
        auto const final_origin = locator.segment_origin(final_segment_index);
        auto const final_value = spline.segments[final_segment_index](locator.x_max(), final_origin);
        if (spline.extend_final_tangent.y0 != final_value)
        {
            return {.error = spline_validation_error_t::tangent_anchor, .segment_index = final_segment_index};
        }

        return {};
    }
};

} // namespace crv::spline
