// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
#include <array>
#include <cassert>

namespace crv::pipeline {

/// applies rotation/anisotropy, scalar curve gain, and output-DPI normalization
template <is_fixed t_gain_t>
    requires(is_signed_v<t_gain_t>)
struct output_transform_t
{
    using input_t = int32_t;
    using gain_t = t_gain_t;

    // signed Q10.53 covers matrix terms through anisotropy 1000
    using coefficient_t = fixed_t<int64_t, 53>;
    using row_t = std::array<coefficient_t, 2>;
    using matrix_t = std::array<row_t, 2>;

    // |axis| < 2^20 and |matrix term| <= 1000 fit each result in signed Q31.32
    using transform_t = fixed_t<int64_t, 32>;

    // gain comes last so residual accumulation can use the wide product
    using out_t = fixed_t<int128_t, gain_t::frac_bits>;

    // use the final output headroom for DPI scaling while retaining Q53 output precision
    static constexpr auto scale_integer_bits = out_t::int_bits - transform_t::int_bits - gain_t::int_bits;
    static_assert(0 < scale_integer_bits && scale_integer_bits < 64);
    using scale_t = fixed_t<uint64_t, 64 - scale_integer_bits>;
    static constexpr auto max_scale_integer = (uint64_t{1} << scale_integer_bits) - 1;

    static constexpr auto input_limit = input_t{1} << 20;

    struct result_t
    {
        out_t x{};
        out_t y{};
        bool valid = false;

        constexpr auto operator==(result_t const&) const noexcept -> bool = default;
    };

    matrix_t matrix{{
        {coefficient_t{1}, coefficient_t{}},
        {coefficient_t{}, coefficient_t{1}},
    }};
    scale_t output_scale{1};

    constexpr auto rotation_components_are_valid() const noexcept -> bool
    {
        auto const& row = matrix[0];
        auto const limit = one + coefficient_ulp;
        return component_in_range(row[0], limit) && component_in_range(row[1], limit);
    }

    constexpr auto anisotropy_components_are_valid() const noexcept -> bool
    {
        auto const& row = matrix[1];
        auto const limit = max_anisotropy + anisotropy_coefficient_tolerance;
        return component_in_range(row[0], limit) && component_in_range(row[1], limit);
    }

    constexpr auto rotation_norm_is_valid() const noexcept -> bool
    {
        return rotation_components_are_valid() && row_norm_is_unit(matrix[0]);
    }

    constexpr auto anisotropy_norm_is_valid() const noexcept -> bool
    {
        return anisotropy_components_are_valid()
            && row_norm_squared(matrix[1]) <= row_norm_limit_squared(max_anisotropy, anisotropy_coefficient_tolerance);
    }

    constexpr auto rows_are_orthogonal() const noexcept -> bool
    {
        return rotation_components_are_valid() && anisotropy_components_are_valid()
            && rows_are_orthogonal(matrix[0], matrix[1]);
    }

    constexpr auto determinant_is_positive() const noexcept -> bool
    {
        return rotation_components_are_valid() && anisotropy_components_are_valid()
            && determinant(matrix[0], matrix[1]) > matrix_product_t{};
    }

    constexpr auto output_scale_is_valid() const noexcept -> bool { return output_scale > scale_t{}; }

    constexpr auto operator()(input_t x, input_t y, gain_t gain) const noexcept -> result_t
    {
        if (!input_in_range(x) || !input_in_range(y)) return {};

        auto const& [x_row, y_row] = matrix;

        assert(rotation_components_are_valid() && "output_transform_t: rotation row component outside range");
        assert(anisotropy_components_are_valid() && "output_transform_t: anisotropy row component outside range");
        assert(rotation_norm_is_valid() && "output_transform_t: rotation row norm must be one");
        assert(anisotropy_norm_is_valid() && "output_transform_t: anisotropy outside supported range");
        assert(rows_are_orthogonal() && "output_transform_t: matrix rows must be orthogonal");
        assert(determinant_is_positive() && "output_transform_t: anisotropy must be positive");
        assert(output_scale_is_valid() && "output_transform_t: output scale must be positive");

        using axis_t = fixed_t<int64_t, 0>;
        auto const fx = axis_t{x};
        auto const fy = axis_t{y};

        auto const x0 = multiply(fx, x_row[0]);
        auto const x1 = multiply(fy, x_row[1]);
        auto const y0 = multiply(fx, y_row[0]);
        auto const y1 = multiply(fy, y_row[1]);

        auto const transformed_x = transform_t::template convert<rounding_mode>(x0 + x1);
        auto const transformed_y = transform_t::template convert<rounding_mode>(y0 + y1);

        auto const gained_x = out_t::template convert<rounding_mode>(multiply(transformed_x, gain));
        auto const gained_y = out_t::template convert<rounding_mode>(multiply(transformed_y, gain));

        return {
            .x = apply_output_scale(gained_x),
            .y = apply_output_scale(gained_y),
            .valid = true,
        };
    }

private:
    constexpr auto apply_output_scale(out_t input) const noexcept -> out_t
    {
        using unsigned_out_value_t = make_unsigned_t<typename out_t::value_t>;
        static constexpr auto shift = scale_t::frac_bits;
        static constexpr auto mask = (unsigned_out_value_t{1} << shift) - 1;
        static constexpr auto half = unsigned_out_value_t{1} << (shift - 1);

        auto const quotient = input.value >> shift;
        auto const remainder = static_cast<unsigned_out_value_t>(input.value) & mask;
        auto const scale = static_cast<unsigned_out_value_t>(output_scale.value);
        auto const fractional_product = remainder * scale;
        auto const fractional_whole = fractional_product >> shift;
        auto const fractional_remainder = fractional_product & mask;

        auto scaled = quotient * int_cast<typename out_t::value_t>(output_scale.value)
            + int_cast<typename out_t::value_t>(fractional_whole);
        if (fractional_remainder > half || (fractional_remainder == half && (scaled & 1) != 0)) ++scaled;

        return out_t::literal(scaled);
    }

    static constexpr auto input_in_range(input_t value) noexcept -> bool
    {
        return -input_limit < value && value < input_limit;
    }

    static constexpr auto component_in_range(coefficient_t value, coefficient_t limit) noexcept -> bool
    {
        return -limit <= value && value <= limit;
    }

    using matrix_product_t = fixed::product_t<coefficient_t, coefficient_t>;

    static constexpr auto row_norm_squared(row_t const& row) noexcept -> matrix_product_t
    {
        return multiply(row[0], row[0]) + multiply(row[1], row[1]);
    }

    static constexpr auto row_norm_limit_squared(coefficient_t limit, coefficient_t tolerance) noexcept
        -> matrix_product_t
    {
        auto const upper = limit + tolerance;
        return multiply(upper, upper);
    }

    static constexpr auto row_norm_is_unit(row_t const& row) noexcept -> bool
    {
        auto const lower = one - rotation_norm_tolerance;
        auto const upper = one + rotation_norm_tolerance;
        auto const norm = row_norm_squared(row);
        return multiply(lower, lower) <= norm && norm <= multiply(upper, upper);
    }

    static constexpr auto row_dot(row_t const& lhs, row_t const& rhs) noexcept -> matrix_product_t
    {
        return multiply(lhs[0], rhs[0]) + multiply(lhs[1], rhs[1]);
    }

    static constexpr auto rows_are_orthogonal(row_t const& lhs, row_t const& rhs) noexcept -> bool
    {
        auto const dot = row_dot(lhs, rhs);
        return -orthogonality_tolerance <= dot && dot <= orthogonality_tolerance;
    }

    static constexpr auto determinant(row_t const& x_row, row_t const& y_row) noexcept -> matrix_product_t
    {
        return multiply(x_row[0], y_row[1]) - multiply(x_row[1], y_row[0]);
    }

    static constexpr auto rounding_mode = rounding_modes::shr::fast::nearest_away;
    static constexpr auto one = coefficient_t{1};
    static constexpr auto coefficient_ulp = coefficient_t::literal(1);
    static constexpr auto rotation_norm_tolerance = coefficient_t::literal(2);
    static constexpr auto max_anisotropy = coefficient_t{1000};
    // binary64 spacing near 1000 is 1024 Q53 ulps; leave four ulps of generator headroom
    static constexpr auto anisotropy_coefficient_tolerance = coefficient_t::literal(4096);
    static constexpr auto max_anisotropy_with_tolerance = max_anisotropy + anisotropy_coefficient_tolerance;
    static constexpr auto max_anisotropy_raw = static_cast<uint128_t>(max_anisotropy_with_tolerance.value);

    static constexpr auto orthogonality_error_per_component = multiply(one, anisotropy_coefficient_tolerance)
        + multiply(max_anisotropy, coefficient_ulp) + multiply(coefficient_ulp, anisotropy_coefficient_tolerance);
    static constexpr auto orthogonality_tolerance
        = orthogonality_error_per_component + orthogonality_error_per_component;

    static_assert(max_anisotropy_raw <= static_cast<uint128_t>(max<int128_t>()) / uint128_t{2} / max_anisotropy_raw,
        "output_transform_t: anisotropy row norm can overflow int128");
    static_assert(out_t::int_bits == transform_t::int_bits + gain_t::int_bits + scale_t::int_bits,
        "output_transform_t: output scale must consume only final output headroom");
};

} // namespace crv::pipeline
