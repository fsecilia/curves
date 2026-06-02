// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/test/test.hpp>

#include <iostream>

namespace crv::signal_chain {
namespace {

/// scales input before forwarding
template <typename real_t, typename prev_t> struct input_scale_t
{
    real_t scale;
    prev_t prev;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return prev(x * scale);
    }
};

/// offsets input before forwarding
template <typename real_t, typename prev_t> struct input_offset_t
{
    real_t offset;
    prev_t prev;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return prev(x - offset);
    }
};

/// stand-in until we have derivative blends ready
struct linear_transition_t
{
    template <typename scalar_t>
    constexpr auto operator()(scalar_t t, scalar_t y0, scalar_t y1) const noexcept -> scalar_t
    {
        assert(scalar_t{0.0} <= t && t <= scalar_t{1.0});
        return (scalar_t{1} - t) * y0 + t * y1;
    }
};

/// applies transition func between two functions at an offset over a width
template <typename real_t, typename prev_t, typename next_t, typename transition_func_t> struct transition_t
{
    real_t offset;
    real_t reciprocal_width;
    prev_t prev;
    next_t next;
    transition_func_t transition_func;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        using std::clamp;
        auto const t = clamp((x - offset) * reciprocal_width, scalar_t{0.0}, scalar_t{1.0});
        return transition_func(t, prev(x), next(x));
    }
};

/// scales input before forwarding
template <typename real_t, typename prev_t> struct output_scale_t
{
    real_t scale;
    prev_t prev;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return prev(x) * scale;
    }
};

/// offsets output
template <typename real_t, typename prev_t> struct output_offset_t
{
    real_t offset;
    prev_t prev;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return prev(x) + offset;
    }
};

TEST(signal_chain_test, integration)
{
    using real_t = float_t; // == float64_t
    using scalar_t = real_t;

    auto const input_scale = real_t{1.25};
    auto const output_scale = real_t{0.65};

    auto const x0 = 0.5;
    auto const x1 = 1;
    auto const x2 = 3;
    auto const x3 = 4;
    auto const y0 = 0.25;
    auto const y3 = 17;
    auto const input_horizontal = [](scalar_t) noexcept -> scalar_t { return scalar_t{0}; };
    auto const base_function = [](scalar_t x) noexcept -> scalar_t { return x * x; };
    auto const output_horizontal = [&](scalar_t) noexcept -> scalar_t { return y3; };

    using input_horizontal_t = std::remove_cvref_t<decltype(input_horizontal)>;
    using base_function_t = std::remove_cvref_t<decltype(base_function)>;
    using output_horizontal_t = std::remove_cvref_t<decltype(output_horizontal)>;

    using input_scale_t = input_scale_t<real_t, base_function_t>;
    using input_offset_t = input_offset_t<real_t, input_scale_t>;
    using input_transition_t = transition_t<real_t, input_horizontal_t, input_offset_t, linear_transition_t>;
    using output_scale_t = output_scale_t<real_t, input_transition_t>;
    using output_offset_t = output_offset_t<real_t, output_scale_t>;
    using output_transition_t = transition_t<real_t, output_offset_t, output_horizontal_t, linear_transition_t>;

    auto unlimited_chain = output_offset_t{
        .offset = y0,
        .prev = output_scale_t {
            .scale = output_scale,
            .prev = input_transition_t{
                .offset = x0,
                .reciprocal_width = real_t{1.0} / (x1 - x0),
                .prev = input_horizontal,
                .next = input_offset_t{
                    .offset = x0,
                    .prev = input_scale_t{
                        .scale = input_scale,
                        .prev = base_function,
                    },
                },
                .transition_func = linear_transition_t{},
            }
        }
    };

    // determine limits; for now just hardcode

    auto const output_chain = output_transition_t{.offset = x2,
        .reciprocal_width = real_t{1.0} / (x3 - x2),
        .prev = std::move(unlimited_chain),
        .next = output_horizontal,
        .transition_func = linear_transition_t{}};

    auto const dx = 0.1;
    for (auto x = real_t{0.0}; x < x3 + dx * 3; x += dx) { std::cout << x << ", " << output_chain(x) << "\n"; }
    std::cout.flush();
}

} // namespace
} // namespace crv::signal_chain
