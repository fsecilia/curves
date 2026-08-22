// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>

namespace crv::pipeline {

/// independent x/y fractional carry with reserve/commit updates
template <is_fixed t_input_t, integral t_output_t = int32_t>
    requires(is_signed_v<t_input_t> && is_signed_v<t_output_t>)
class residual_accumulator_t
{
public:
    using input_t = t_input_t;
    using output_t = t_output_t;
    using residual_t = fixed_t<int64_t, input_t::frac_bits>;

    static_assert(input_t::frac_bits < residual_t::container_bits - 1,
        "residual_accumulator_t: residual radix does not fit int64");

    struct reservation_t
    {
        output_t x{};
        output_t y{};
        residual_t x_residual{};
        residual_t y_residual{};
        bool valid = false;

        constexpr auto operator==(reservation_t const&) const noexcept -> bool = default;
    };

    constexpr auto x_residual() const noexcept -> residual_t { return x_residual_; }
    constexpr auto y_residual() const noexcept -> residual_t { return y_residual_; }

    /// reserves quantized output and next residuals without changing state
    constexpr auto reserve(input_t x, input_t y) const noexcept -> reservation_t
    {
        auto const x_reservation = make_axis_reservation(x, x_residual_);
        auto const y_reservation = make_axis_reservation(y, y_residual_);

        if (!x_reservation.valid || !y_reservation.valid) return {};

        return {
            .x = x_reservation.output,
            .y = y_reservation.output,
            .x_residual = x_reservation.residual,
            .y_residual = y_reservation.residual,
            .valid = true,
        };
    }

    /// commits a reservation after the report update succeeds
    constexpr auto commit(reservation_t const& reservation) noexcept -> void
    {
        assert(reservation.valid && "residual_accumulator_t: cannot commit invalid reservation");
        x_residual_ = reservation.x_residual;
        y_residual_ = reservation.y_residual;
    }

private:
    struct axis_reservation_t
    {
        output_t output{};
        residual_t residual{};
        bool valid = false;
    };

    constexpr auto make_axis_reservation(input_t input, residual_t residual) const noexcept -> axis_reservation_t
    {
        using wide_t = typename input_t::value_t;
        auto const residual_wide = int_cast<wide_t>(residual.value);
        auto const z = add_wrap(input.value, residual_wide);

        auto const rounded = shifter.template shr<input_t::frac_bits>(z);
        if (!in_range<output_t>(rounded)) return {};

        auto const output = static_cast<output_t>(rounded);
        auto const output_fixed = shifter.template shl<wide_t, input_t::frac_bits>(output);
        auto const residual_value = subtract_wrap(z, output_fixed);

        assert(in_range<typename residual_t::value_t>(residual_value)
            && "residual_accumulator_t: residual does not fit state representation");

        return {
            .output = output,
            .residual = residual_t::literal(static_cast<typename residual_t::value_t>(residual_value)),
            .valid = true,
        };
    }

    static constexpr auto shifter = shifter_t<rounding_modes::shr::nearest_even>{};

    residual_t x_residual_{};
    residual_t y_residual_{};
};

} // namespace crv::pipeline
