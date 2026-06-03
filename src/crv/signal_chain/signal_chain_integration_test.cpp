// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/signal_chain/curves/derivatives.hpp>
#include <crv/signal_chain/quadrature/adaptive_integrator.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <numeric>
#include <optional>
#include <utility>

namespace crv::signal_chain {

using model::curves::derivatives_t;

struct c4_transition_t
{
    template <typename value_t> constexpr auto evaluate(value_t t) const noexcept -> value_t
    {
        if (t <= value_t{0.0}) return value_t{0.0};
        if (t >= value_t{1.0}) return value_t{1.0};

        auto const t2 = t * t;
        auto const t4 = t2 * t2;

        // W(t) = 70t^9 - 315t^8 + 540t^7 - 420t^6 + 126t^5
        return t4 * t
            * (value_t{126.0}
                + t * (value_t{-420.0} + t * (value_t{540.0} + t * (value_t{-315.0} + t * value_t{70.0}))));
    }

    template <int order, typename value_t>
    constexpr auto derivatives(value_t t) const noexcept -> derivatives_t<order, value_t>
    {
        auto d = derivatives_t<order, value_t>{};

        if (t <= value_t{0.0})
        {
            // f, d1, d2, d3 are natively 0.0 due to default construction
            return d;
        }

        if (t >= value_t{1.0})
        {
            d.f = value_t{1.0};
            // d1, d2, d3 remain 0.0
            return d;
        }

        d.f = evaluate(t);

        if constexpr (order >= 1)
        {
            // W'(t) = 630t^4(1 - t)^4
            // Factored specifically for numerical stability near the edges
            auto const t_inv = value_t{1.0} - t;
            auto const t2 = t * t;
            auto const t_inv2 = t_inv * t_inv;

            d.d1 = value_t{630.0} * (t2 * t2) * (t_inv2 * t_inv2);
        }

        if constexpr (order >= 2)
        {
            // W''(t) = 5040t^7 - 17640t^6 + 22680t^5 - 12600t^4 + 2520t^3
            auto const t2 = t * t;

            d.d2 = t2 * t
                * (value_t{2520.0}
                    + t * (value_t{-12600.0} + t * (value_t{22680.0} + t * (value_t{-17640.0} + t * value_t{5040.0}))));
        }

        if constexpr (order >= 3)
        {
            // W'''(t) = 35280t^6 - 105840t^5 + 113400t^4 - 50400t^3 + 7560t^2
            auto const t2 = t * t;

            d.d3 = t2
                * (value_t{7560.0}
                    + t
                        * (value_t{-50400.0}
                            + t * (value_t{113400.0} + t * (value_t{-105840.0} + t * value_t{35280.0}))));
        }

        return d;
    }
};

//
// stages
//

/// scales input before forwarding
template <typename real_t, typename prev_t> struct input_scale_t
{
    real_t scale;
    prev_t prev;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return prev(x * scale);
    }

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = prev.template derivatives<order>(x * scale);

        if constexpr (order >= 1) d.d1 *= scale;
        if constexpr (order >= 2) d.d2 *= (scale * scale);
        if constexpr (order >= 3) d.d3 *= (scale * scale * scale);

        return d;
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

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        return prev.template derivatives<order>(x - offset); // shift, no scale change
    }
};

/// scales output
template <typename real_t, typename prev_t> struct output_scale_t
{
    real_t scale;
    prev_t prev;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return prev(x) * scale;
    }

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = prev.template derivatives<order>(x);
        d.f *= scale;
        if constexpr (order >= 1) d.d1 *= scale;
        if constexpr (order >= 2) d.d2 *= scale;
        if constexpr (order >= 3) d.d3 *= scale;
        return d;
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

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = prev.template derivatives<order>(x);

        // shift only 0th order value
        d.f += offset;

        return d;
    }
};

// =============================================================================
// 2. SOLVER CORE
// =============================================================================

template <std::floating_point real_t, typename monotone_fn_t>
[[nodiscard]] constexpr auto bisect_lower_bound(real_t lo, real_t hi, real_t target, monotone_fn_t const& f) noexcept
    -> real_t
{
    while (true)
    {
        auto const mid = std::midpoint(lo, hi);
        if (mid == lo || mid == hi) break;

        if (f(mid) < target) lo = mid;
        else hi = mid;
    }
    return hi;
}

enum class placement_status_t
{
    found,
    above_range,
    below_range
};

template <std::floating_point t_real_t> struct placement_t
{
    using real_t = t_real_t;
    real_t x3;
    real_t x2;
    real_t y2;
    constexpr auto operator==(placement_t const&) const noexcept -> bool = default;
};

template <std::floating_point t_real_t> struct placement_result_t
{
    using real_t = t_real_t;
    using placement_type = placement_t<real_t>;
    placement_status_t status;
    std::optional<placement_type> placement;

    [[nodiscard]] constexpr auto engaged() const noexcept -> bool { return status == placement_status_t::found; }
    constexpr auto operator==(placement_result_t const&) const noexcept -> bool = default;

    static constexpr auto found(placement_type p) noexcept -> placement_result_t
    {
        return {.status = placement_status_t::found, .placement = p};
    }
    static constexpr auto above_range() noexcept -> placement_result_t
    {
        return {.status = placement_status_t::above_range, .placement = std::nullopt};
    }
    static constexpr auto below_range() noexcept -> placement_result_t
    {
        return {.status = placement_status_t::below_range, .placement = std::nullopt};
    }
};

template <std::floating_point t_real_t> class placement_solver_t
{
public:
    using real_t = t_real_t;
    using result_t = placement_result_t<real_t>;

    constexpr placement_solver_t(real_t domain_lo, real_t domain_hi) noexcept
        : domain_lo_{domain_lo}, domain_hi_{domain_hi}
    {}

    template <typename model_t>
    [[nodiscard]] constexpr auto operator()(model_t const& model, real_t target_gain) const -> result_t
    {
        auto const x2_lo = domain_lo_;
        auto const x2_hi = model.max_x2(domain_hi_);
        assert(x2_lo <= x2_hi && "placement_solver_t: knee wider than domain");

        if (target_gain <= model.cap(x2_lo)) return result_t::below_range();
        if (model.cap(x2_hi) < target_gain) return result_t::above_range();

        auto const cap = [&model](real_t x2) { return model.cap(x2); };
        auto const x2 = bisect_lower_bound(x2_lo, x2_hi, target_gain, cap);

        return result_t::found(model.placement(x2));
    }

private:
    real_t domain_lo_;
    real_t domain_hi_;
};

template <std::floating_point t_real_t, typename t_subchain_t, typename t_subchain_derivative_t,
    typename t_transition_t, typename t_rule_t = quadrature::rules::gauss_kronrod_t<t_real_t>>
class knee_model_t
{
public:
    using real_t = t_real_t;
    using subchain_t = t_subchain_t;
    using subchain_derivative_t = t_subchain_derivative_t;
    using transition_t = t_transition_t;
    using rule_t = t_rule_t;

    constexpr knee_model_t(subchain_t const& subchain, subchain_derivative_t const& subchain_derivative,
        transition_t const& transition, real_t w_l, rule_t rule = {}) noexcept
        : subchain_{subchain}, subchain_derivative_{subchain_derivative}, transition_{transition}, w_l_{w_l},
          rule_{std::move(rule)}
    {}

    [[nodiscard]] constexpr auto cap(real_t x2) const -> real_t { return subchain_(x2) + w_l_ * deficit(x2); }
    [[nodiscard]] constexpr auto placement(real_t x2) const -> placement_t<real_t>
    {
        return {.x3 = x2 + w_l_, .x2 = x2, .y2 = subchain_(x2)};
    }
    [[nodiscard]] constexpr auto max_x2(real_t domain_hi) const noexcept -> real_t { return domain_hi - w_l_; }
    [[nodiscard]] constexpr auto width() const noexcept -> real_t { return w_l_; }

private:
    [[nodiscard]] constexpr auto deficit(real_t x2) const -> real_t
    {
        auto const integrand = [this, x2](real_t s) noexcept {
            return subchain_derivative_(x2 + w_l_ * s) * (real_t{1} - transition_.evaluate(s));
        };

        return rule_.integrate(real_t{0.0}, real_t{1.0}, integrand);
    }

    subchain_t const& subchain_;
    subchain_derivative_t const& subchain_derivative_;
    transition_t const& transition_;
    real_t w_l_;
    [[no_unique_address]] rule_t rule_;
};

namespace detail {
template <std::floating_point real_t, typename inner_derivative_t, typename weight_t, typename rule_t>
[[nodiscard]] constexpr auto build_blend_antiderivative(inner_derivative_t inner_derivative, real_t x_start, real_t w,
    weight_t weight, real_t tolerance, int_t depth_limit, rule_t rule)
{
    static constexpr auto no_critical_points = std::array<real_t, 0>{};
    auto const integrand = [inner_derivative = std::move(inner_derivative), weight = std::move(weight), x_start, w](
                               real_t s) noexcept { return inner_derivative(x_start + w * s) * weight(s); };
    auto integrator = quadrature::adaptive_integrator_t<real_t>{tolerance, depth_limit};
    auto result = integrator(quadrature::integral_t{integrand, std::move(rule)}, real_t{1}, no_critical_points);
    return std::move(result.antiderivative);
}
} // namespace detail

// =============================================================================
// 3. TRANSITION MATH NODES
// =============================================================================

template <typename real_t, typename prev_t, typename transition_fn_t, typename antiderivative_t> struct offset_taper_t
{
    real_t start;
    real_t width;
    real_t reciprocal_width;
    real_t floor_value;
    real_t deficit;
    prev_t prev;
    transition_fn_t transition_fn;
    antiderivative_t antiderivative;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        // s in (0,1):  floor + width*antiderivative(s)
        // s >= 1:      prev(x) + floor - deficit
        //   where deficit = prev(start+width) - width*antiderivative(1)
        //   so at s==1 both branches equal floor + width*antiderivative(1).  C1 by construction.

        auto const s = (x - start) * reciprocal_width;
        if (s <= value_t{0}) return static_cast<value_t>(floor_value);
        if (s >= value_t{1}) return prev(x) + static_cast<value_t>(floor_value - deficit);
        return static_cast<value_t>(floor_value + width * antiderivative(static_cast<real_t>(s)));
    }

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = derivatives_t<order, scalar_t>{};
        d.f = this->operator()(x);
        if constexpr (order == 0) return d;

        auto const s = (x - start) * reciprocal_width;

        if (s <= scalar_t{0})
        {
            // Floor region: all derivatives are 0
            if constexpr (order >= 1) d.d1 = scalar_t{0};
            if constexpr (order >= 2) d.d2 = scalar_t{0};
            if constexpr (order >= 3) d.d3 = scalar_t{0};
        }
        else if (s >= scalar_t{1})
        {
            // Above region: pure curve, un-weighted
            auto prev_d = prev.template derivatives<order>(x);
            if constexpr (order >= 1) d.d1 = prev_d.d1;
            if constexpr (order >= 2) d.d2 = prev_d.d2;
            if constexpr (order >= 3) d.d3 = prev_d.d3;
        }
        else
        {
            // Inside region: The Product Rule
            auto prev_d = prev.template derivatives<order>(x);

            // The transition function is always evaluated one order lower than the curve.
            constexpr int t_order = (order > 0) ? order - 1 : 0;
            auto T = transition_fn.template derivatives<t_order>(static_cast<real_t>(s));

            if constexpr (order >= 1) { d.d1 = prev_d.d1 * T.f; }
            if constexpr (order >= 2)
            {
                d.d2 = (prev_d.d2 * T.f) + (prev_d.d1 * T.d1 * reciprocal_width);

                if constexpr (order >= 3)
                {
                    auto const rw2 = reciprocal_width * reciprocal_width;
                    d.d3 = (prev_d.d3 * T.f) + (scalar_t{2} * prev_d.d2 * T.d1 * reciprocal_width)
                        + (prev_d.d1 * T.d2 * rw2);
                }
            }
        }

        return d;
    }
};

template <typename real_t, typename prev_t, typename transition_fn_t, typename antiderivative_t> struct limiter_t
{
    real_t knee_start;
    real_t width;
    real_t reciprocal_width;
    real_t cap;
    prev_t prev;
    transition_fn_t transition_fn;
    antiderivative_t antiderivative;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        auto const s = (x - knee_start) * reciprocal_width;
        if (s <= value_t{0}) return prev(x);
        if (s >= value_t{1}) return static_cast<value_t>(cap);
        return static_cast<value_t>(prev(knee_start) + width * antiderivative(static_cast<real_t>(s)));
    }

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = derivatives_t<order, scalar_t>{};
        d.f = this->operator()(x);
        if constexpr (order == 0) return d;

        auto const s = (x - knee_start) * reciprocal_width;

        if (s <= scalar_t{0})
        {
            // Base curve region: pure curve
            auto prev_d = prev.template derivatives<order>(x);
            if constexpr (order >= 1) d.d1 = prev_d.d1;
            if constexpr (order >= 2) d.d2 = prev_d.d2;
            if constexpr (order >= 3) d.d3 = prev_d.d3;
        }
        else if (s >= scalar_t{1})
        {
            // Cap region: f is locked, all derivatives are 0
            if constexpr (order >= 1) d.d1 = scalar_t{0};
            if constexpr (order >= 2) d.d2 = scalar_t{0};
            if constexpr (order >= 3) d.d3 = scalar_t{0};
        }
        else
        {
            // Inside region: The Product Rule for (1 - W(t)) * g'(x)
            auto prev_d = prev.template derivatives<order>(x);

            constexpr int t_order = (order > 0) ? order - 1 : 0;
            auto T = transition_fn.template derivatives<t_order>(static_cast<real_t>(s));

            auto const v = scalar_t{1} - T.f;

            if constexpr (order >= 1) { d.d1 = prev_d.d1 * v; }
            if constexpr (order >= 2)
            {
                // Note the negative signs because we are differentiating (1 - W(t))
                d.d2 = (prev_d.d2 * v) - (prev_d.d1 * T.d1 * reciprocal_width);

                if constexpr (order >= 3)
                {
                    auto const rw2 = reciprocal_width * reciprocal_width;
                    d.d3 = (prev_d.d3 * v) - (scalar_t{2} * prev_d.d2 * T.d1 * reciprocal_width)
                        - (prev_d.d1 * T.d2 * rw2);
                }
            }
        }

        return d;
    }
};

// =============================================================================
// 4. AST FACTORIES
// =============================================================================

template <typename real_t, typename prev_t> constexpr auto make_input_scale(real_t scale, prev_t prev)
{
    return input_scale_t<real_t, std::remove_cvref_t<prev_t>>{scale, std::move(prev)};
}
template <typename real_t, typename prev_t> constexpr auto make_input_offset(real_t offset, prev_t prev)
{
    return input_offset_t<real_t, std::remove_cvref_t<prev_t>>{offset, std::move(prev)};
}
template <typename real_t, typename prev_t> constexpr auto make_output_scale(real_t scale, prev_t prev)
{
    return output_scale_t<real_t, std::remove_cvref_t<prev_t>>{scale, std::move(prev)};
}
template <typename real_t, typename prev_t> constexpr auto make_output_offset(real_t offset, prev_t prev)
{
    return output_offset_t<real_t, std::remove_cvref_t<prev_t>>{offset, std::move(prev)};
}

template <std::floating_point real_t, typename prev_t, typename transition_fn_t,
    typename rule_t = quadrature::rules::gauss_kronrod_t<real_t>>
[[nodiscard]] constexpr auto make_offset_taper(prev_t prev, real_t start, real_t width, real_t floor_value,
    transition_fn_t const& transition_fn, real_t tolerance, int_t depth_limit, rule_t rule = {})
{
    auto const prev_derivative = [prev](real_t x) noexcept { return prev.template derivatives<1>(x).d1; };
    auto const weight = [transition_fn](real_t s) noexcept { return transition_fn.evaluate(s); };

    auto antiderivative = detail::build_blend_antiderivative<real_t>(
        prev_derivative, start, width, weight, tolerance, depth_limit, rule);
    auto const local_integral = antiderivative(real_t{1});
    auto const deficit = prev(start + width) - (width * local_integral);

    using antiderivative_t = std::remove_cvref_t<decltype(antiderivative)>;
    using result_node_t = offset_taper_t<real_t, prev_t, transition_fn_t, antiderivative_t>;

    return result_node_t{.start = start,
        .width = width,
        .reciprocal_width = real_t{1} / width,
        .floor_value = floor_value,
        .deficit = deficit,
        .prev = std::move(prev),
        .transition_fn = std::move(transition_fn),
        .antiderivative = std::move(antiderivative)};
}

template <std::floating_point real_t, typename prev_t, typename transition_fn_t,
    typename rule_t = quadrature::rules::gauss_kronrod_t<real_t>>
[[nodiscard]] constexpr auto make_limiter(prev_t prev, real_t limit_gain, real_t width, real_t domain_lo,
    real_t domain_hi, transition_fn_t transition_fn, real_t tolerance, int_t depth_limit, rule_t rule = {})
{
    auto const prev_derivative = [prev](real_t x) noexcept { return prev.template derivatives<1>(x).d1; };
    auto const weight = [transition_fn](real_t s) noexcept { return real_t{1} - transition_fn.evaluate(s); };

    using model_t = knee_model_t<real_t, prev_t, decltype(prev_derivative), transition_fn_t, rule_t>;
    using antiderivative_t = std::remove_cvref_t<decltype(detail::build_blend_antiderivative<real_t>(
        prev_derivative, real_t{0}, width, weight, tolerance, depth_limit, rule))>;
    using result_node_t = limiter_t<real_t, prev_t, transition_fn_t, antiderivative_t>;
    using return_t = std::optional<result_node_t>;

    auto const model = model_t{prev, prev_derivative, transition_fn, width, rule};
    auto const solver = placement_solver_t<real_t>{domain_lo, domain_hi};
    auto const placement = solver(model, limit_gain);

    if (!placement.engaged()) return return_t{std::nullopt};

    auto const knee_start = placement.placement->x2;
    auto antiderivative = detail::build_blend_antiderivative<real_t>(
        prev_derivative, knee_start, width, weight, tolerance, depth_limit, rule);
    auto const cap = prev(knee_start) + width * antiderivative(real_t{1});

    assert(std::abs(cap - limit_gain) <= tolerance * (real_t{1} + std::abs(limit_gain))
        && "make_limiter: rebuilt cap diverged from solved target");

    return return_t{result_node_t{.knee_start = knee_start,
        .width = width,
        .reciprocal_width = real_t{1} / width,
        .cap = cap,
        .prev = std::move(prev),
        .transition_fn = std::move(transition_fn),
        .antiderivative = std::move(antiderivative)}};
}

// =============================================================================
// TEST MOCKS & RUNNER
// =============================================================================

struct mock_quadratic_t
{
    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t { return x * x; }
    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = derivatives_t<order, scalar_t>{};
        d.f = x * x;
        if constexpr (order >= 1) d.d1 = scalar_t{2} * x;
        if constexpr (order >= 2) d.d2 = scalar_t{2};
        if constexpr (order >= 3) d.d3 = scalar_t{0};
        return d;
    }
};

TEST(signal_chain_test, integration)
{
    using real_t = float_t;
    // using scalar_t = real_t;

    auto const smooth_blend = c4_transition_t{};
    auto const rule = quadrature::rules::gauss_kronrod_t<real_t>{};
    auto const base_curve = mock_quadratic_t{};

    // Note: Single seamless chain construction. The compiler tracks ALL d for you.
    auto inner_chain = make_input_offset(0.5, make_input_scale(1.25, base_curve));

    auto taper = make_offset_taper<real_t>(std::move(inner_chain),
        /*start=*/0.5,
        /*width=*/0.5,
        /*floor=*/0.25, smooth_blend, 1e-6, 10, rule);

    auto unlimited_chain = make_output_offset(0.25, make_output_scale(0.65, std::move(taper)));

    auto output_chain_opt = make_limiter<real_t>(std::move(unlimited_chain),
        /*limit_gain=*/17.0,
        /*limiter_width=*/1.0,
        /*domain_lo=*/0.0,
        /*domain_hi=*/100.0, smooth_blend, 1e-6, 10, rule);

    ASSERT_TRUE(output_chain_opt.has_value());
    auto const& output_chain = *output_chain_opt;

    auto const dx = 0.1;
    for (auto x = real_t{0.0}; x < 5.0 + dx * 3; x += dx) { std::cout << x << ", " << output_chain(x) << "\n"; }
    std::cout.flush();
}

} // namespace crv::signal_chain
