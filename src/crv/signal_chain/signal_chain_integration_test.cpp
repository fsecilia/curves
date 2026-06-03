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

struct cinf_transition_t
{
    template <typename scalar_t> auto evaluate(scalar_t t) const noexcept -> scalar_t
    {
        if (t <= scalar_t{0.0}) return scalar_t{0.0};
        if (t >= scalar_t{1.0}) return scalar_t{1.0};

        using std::exp;
        auto const q_t = exp(scalar_t{-1.0} / t);
        auto const q_1mt = exp(scalar_t{-1.0} / (scalar_t{1.0} - t));

        return q_t / (q_t + q_1mt);
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
        d.f += offset; // only shifts the 0th order value!
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
        transition_t const& transition, real_t w_l, real_t tolerance, int_t depth_limit, rule_t rule = {}) noexcept
        : subchain_{subchain}, subchain_derivative_{subchain_derivative}, transition_{transition}, w_l_{w_l},
          tolerance_{tolerance}, depth_limit_{depth_limit}, rule_{std::move(rule)}
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
        static constexpr auto no_critical_points = std::array<real_t, 0>{};
        auto const integrand = [this, x2](real_t s) noexcept {
            return subchain_derivative_(x2 + w_l_ * s) * (real_t{1} - transition_.evaluate(s));
        };
        auto integrator = quadrature::adaptive_integrator_t<real_t>{tolerance_, depth_limit_};
        auto const result = integrator(quadrature::integral_t{integrand, rule_}, real_t{1}, no_critical_points);
        return result.antiderivative(real_t{1});
    }

    subchain_t const& subchain_;
    subchain_derivative_t const& subchain_derivative_;
    transition_t const& transition_;
    real_t w_l_;
    real_t tolerance_;
    int_t depth_limit_;
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

            auto const T_val = transition_fn.evaluate(static_cast<real_t>(s));
            if constexpr (order >= 1) { d.d1 = prev_d.d1 * T_val; }
            if constexpr (order >= 2)
            {
                auto const T_d1 = transition_fn.derivative(static_cast<real_t>(s));
                d.d2 = (prev_d.d2 * T_val) + (prev_d.d1 * T_d1 * reciprocal_width);

                if constexpr (order >= 3)
                {
                    auto const T_d2 = transition_fn.second_derivative(static_cast<real_t>(s));
                    auto const rw2 = reciprocal_width * reciprocal_width;

                    d.d3 = (prev_d.d3 * T_val) + (scalar_t{2} * prev_d.d2 * T_d1 * reciprocal_width)
                        + (prev_d.d1 * T_d2 * rw2);
                }
            }
        }
        return d;
    }
};

template <typename real_t, typename prev_t, typename antiderivative_t> struct limiter_t
{
    real_t knee_start;
    real_t width;
    real_t reciprocal_width;
    real_t cap;
    prev_t prev;
    antiderivative_t antiderivative;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        auto const s = (x - knee_start) * reciprocal_width;
        if (s <= value_t{0}) return prev(x);
        if (s >= value_t{1}) return static_cast<value_t>(cap);
        return static_cast<value_t>(prev(knee_start) + width * antiderivative(static_cast<real_t>(s)));
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
    using result_node_t = limiter_t<real_t, prev_t, antiderivative_t>;
    using return_t = std::optional<result_node_t>;

    auto const model = model_t{prev, prev_derivative, transition_fn, width, tolerance, depth_limit, rule};
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

    auto const smooth_blend = cinf_transition_t{};
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
