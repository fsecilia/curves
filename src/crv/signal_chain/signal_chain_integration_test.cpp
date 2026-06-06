// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/complex.hpp>
#include <crv/math/complex_traits.hpp>
#include <crv/math/inverse.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/scalar_traits.hpp>
#include <crv/signal_chain/curves/test.hpp>
#include <crv/signal_chain/curves/traits.hpp>
#include <crv/test/test.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace crv::signal_chain {

//
// math primitives & bounds
//

template <std::floating_point real_t>
inline real_t const erf_precision_limit_v = [] {
    using std::log;
    using std::sqrt;
    return sqrt(-log(std::numeric_limits<real_t>::epsilon()));
}();

template <std::floating_point real_t> inline constexpr real_t min_spline_transition_width = real_t{1e-2};

//
// pure geometry (transitions)
//

template <std::floating_point t_real_t> class erf_transition_t
{
public:
    using real_t = t_real_t;

    constexpr erf_transition_t(real_t center, real_t k, real_t half_width) noexcept
        : center_{center}, k_{k}, half_width_{half_width}
    {}

    [[nodiscard]] constexpr auto center() const noexcept -> real_t { return center_; }
    [[nodiscard]] constexpr auto half_width() const noexcept -> real_t { return half_width_; }
    [[nodiscard]] constexpr auto band_low() const noexcept -> real_t { return center_ - half_width_; }
    [[nodiscard]] constexpr auto band_high() const noexcept -> real_t { return center_ + half_width_; }

    template <typename value_t> [[nodiscard]] constexpr auto evaluate(value_t x) const noexcept -> value_t
    {
        using std::real;
        auto const z = static_cast<value_t>(k_) * (x - static_cast<value_t>(center_));

        if (real(z) <= -erf_precision_limit_v<real_t>) return value_t{0};
        if (real(z) >= erf_precision_limit_v<real_t>) return value_t{1};

        return value_t{0.5} * (value_t{1} + complex_step_erf(z));
    }

    [[nodiscard]] constexpr auto integral(real_t x) const noexcept -> real_t
    {
        using std::erf;
        using std::exp;

        auto const z = k_ * (x - center_);

        auto const g = real_t{0.5} * (z * (real_t{1} + erf(z)) + inv_sqrt_pi * exp(-(z * z)));
        return g / k_;
    }

    template <typename value_t> [[nodiscard]] constexpr auto warp(value_t x, real_t offset) const noexcept -> value_t
    {
        auto const p = primal(x);
        auto const phi = (integral(p) - integral(band_low())) + offset;

        if constexpr (is_jet<value_t>) return value_t{phi, evaluate(p) * tangent(x)};
        else return phi;
    }

    [[nodiscard]] constexpr auto peak_d4() const noexcept -> real_t
    {
        auto const abs_k = std::abs(k_);
        return real_t{2.0} * (abs_k * abs_k * abs_k) * inv_sqrt_pi;
    }

private:
    static constexpr auto inv_sqrt_pi = std::numbers::inv_sqrtpi_v<real_t>;

    real_t center_;
    real_t k_;
    real_t half_width_;
};

template <std::floating_point real_t> struct erf_transition_factory_t
{
    using transition_t = erf_transition_t<real_t>;

    [[nodiscard]] constexpr auto make_onset(real_t center, real_t width) const noexcept -> transition_t
    {
        auto const k = calc_k(width);
        return {center, k, calc_half_width(k)};
    }

    [[nodiscard]] constexpr auto make_limit(real_t center, real_t width) const noexcept -> transition_t
    {
        auto const k = -calc_k(width);
        return {center, k, calc_half_width(k)};
    }

private:
    constexpr auto calc_k(real_t width) const noexcept -> real_t
    {
        // (30 sqrt pi)^(1/3) matched from your original PoC to inherit smootherstep splineability
        constexpr auto cube_root_30_sqrt_pi = static_cast<real_t>(3.7603828531416483);
        return cube_root_30_sqrt_pi / width;
    }

    constexpr auto calc_half_width(real_t k) const noexcept -> real_t
    {
        return erf_precision_limit_v<real_t> / std::abs(k);
    }
};

//
// hardware capacity policies
//

template <typename real_t> struct hermite_cubic_policy_t
{
    static constexpr auto max_tolerable_d4(real_t h, real_t epsilon) noexcept -> real_t
    {
        return (real_t{384.0} * epsilon) / (h * h * h * h);
    }
};

//
// structural warps
//

template <typename prev_t, typename transition_t> class onset_warp_t
{
public:
    using real_t = typename transition_t::real_t;

    constexpr onset_warp_t(transition_t transition, prev_t prev)
        : transition_{std::move(transition)}, prev_{std::move(prev)}
    {
        lag_ = transition_.integral(transition_.band_high()) - transition_.integral(transition_.band_low());
    }

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        auto const rx = primal(x);

        if (rx <= transition_.band_low()) return value_t{};
        if (rx < transition_.band_high()) return prev_(transition_.warp(x, real_t{0}));

        return prev_(x - lag_);
    }

    [[nodiscard]] constexpr auto lag() const noexcept -> real_t { return lag_; }

private:
    transition_t transition_;
    real_t lag_;
    prev_t prev_;
};

template <typename prev_t, typename transition_t> class limit_warp_t
{
public:
    using real_t = typename transition_t::real_t;

    constexpr limit_warp_t(transition_t transition, prev_t prev)
        : transition_{std::move(transition)}, prev_{std::move(prev)}
    {
        cap_start_ = transition_.band_low();
        cap_end_ = cap_start_
            + (transition_.integral(transition_.band_high()) - transition_.integral(transition_.band_low()));
    }

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        auto const rx = primal(x);

        if (rx <= transition_.band_low()) return prev_(x);
        if (rx < transition_.band_high()) return prev_(transition_.warp(x, cap_start_));

        return prev_(value_t{cap_end_});
    }

private:
    transition_t transition_;
    real_t cap_start_;
    real_t cap_end_;
    prev_t prev_;
};

//
// affine adapters
//

template <typename real_t, typename prev_t> struct output_scale_t
{
    real_t scale;
    prev_t prev;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return prev(x) * scale;
    }
};

template <typename real_t, typename prev_t> struct output_offset_t
{
    real_t offset;
    prev_t prev;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return prev(x) + offset;
    }
};

template <typename real_t, typename prev_t> constexpr auto make_output_scale(real_t scale, prev_t prev)
{
    return output_scale_t<real_t, std::remove_cvref_t<prev_t>>{scale, std::move(prev)};
}
template <typename real_t, typename prev_t> constexpr auto make_output_offset(real_t offset, prev_t prev)
{
    return output_offset_t<real_t, std::remove_cvref_t<prev_t>>{offset, std::move(prev)};
}

//
// signal chain builder
//

template <typename real_t> struct user_config_t
{
    real_t input_scale;
    real_t input_offset;
    real_t onset_center;
    real_t onset_width;
    real_t limit_target;
    real_t limit_width;
    real_t domain_low;
    real_t domain_high;
};

template <typename real_t> struct grid_params_t
{
    real_t segment_width;
    real_t global_tolerance;
};

enum class builder_error_t
{
    invalid_width,
    warp_overlap,
    excessive_curvature,
    limit_not_reached
};

constexpr auto to_string(builder_error_t error) noexcept -> std::string_view
{
    switch (error)
    {
        case builder_error_t::invalid_width: return "Transition width too small or negative.";
        case builder_error_t::warp_overlap: return "Offset and limit transitions overlap.";
        case builder_error_t::excessive_curvature: return "Transition violates hermite cubic bounds.";
        case builder_error_t::limit_not_reached: return "Base curve never reaches limit target.";
    }
    return "Unknown builder error.";
}

template <typename real_t, typename chain_t> struct builder_result_t
{
    chain_t chain;
    real_t nudged_input_offset;
};

template <typename real_t, real_t min_spline_transition_width> class signal_chain_builder_t
{
    using transition_factory_t = erf_transition_factory_t<real_t>;
    using transition_t = transition_factory_t::transition_t;

    using grid_params_t = grid_params_t<real_t>;

    transition_factory_t transition_factory_;
    bisect_lower_bound_t invert_;
    grid_params_t grid_;

public:
    using user_config_t = user_config_t<real_t>;

    template <typename prev_t>
    using builder_result_t = builder_result_t<real_t, limit_warp_t<onset_warp_t<prev_t, transition_t>, transition_t>>;

    constexpr signal_chain_builder_t(grid_params_t grid) : grid_{grid} {}

    template <typename prev_t>
    [[nodiscard]] auto build(user_config_t const& config, std::optional<real_t> anchor, prev_t prev) const
        -> std::expected<builder_result_t<prev_t>, builder_error_t>
    {
        if (config.onset_width <= min_spline_transition_width || config.limit_width <= min_spline_transition_width)
        {
            return std::unexpected(builder_error_t::invalid_width);
        }

        auto const onset_profile = transition_factory_.make_onset(config.onset_center, config.onset_width);
        auto const limit_test = transition_factory_.make_limit(0.0, config.limit_width);
        auto const max_d4
            = hermite_cubic_policy_t<real_t>::max_tolerable_d4(grid_.segment_width, grid_.global_tolerance);

        if (onset_profile.peak_d4() > max_d4 || limit_test.peak_d4() > max_d4)
        {
            return std::unexpected(builder_error_t::excessive_curvature);
        }

        auto onset_chain = onset_warp_t{onset_profile, std::move(prev)};

        auto const limit_transition_width = limit_test.half_width();
        auto const search_high = config.domain_high + limit_transition_width - onset_chain.lag();
        auto const opt_c_R = invert_(config.domain_low, search_high, config.limit_target, onset_chain);

        auto c_R = opt_c_R.has_value() ? *opt_c_R : (search_high + limit_transition_width); // move past the domain end
        auto nudged_offset = config.input_offset;

        if (anchor)
        {
            auto const ideal_w = (*anchor / config.input_scale) + config.input_offset;
            auto raw_ideal = 0.0;

            if (ideal_w <= c_R)
            {
                if (ideal_w < onset_profile.band_high())
                {
                    raw_ideal = *invert_(onset_profile.band_low(), onset_profile.band_high(), ideal_w, [&](real_t x) {
                        return onset_profile.integral(x) - onset_profile.integral(onset_profile.band_low());
                    });
                }
                else
                {
                    raw_ideal = ideal_w + onset_chain.lag();
                }

                real_t const grid_snapped = std::ceil(raw_ideal / grid_.segment_width) * grid_.segment_width;
                real_t const w_actual = (grid_snapped < onset_profile.band_high())
                    ? onset_profile.integral(grid_snapped) - onset_profile.integral(onset_profile.band_low())
                    : grid_snapped - onset_chain.lag();

                nudged_offset = (config.input_scale * w_actual) - *anchor;
            }
            else
            {
                auto const limit_profile = transition_factory_.make_limit(c_R, config.limit_width);
                raw_ideal = *invert_(limit_profile.band_low(), limit_profile.band_high(), ideal_w, [&](double x) {
                    return limit_profile.band_low()
                        + (limit_profile.integral(x) - limit_profile.integral(limit_profile.band_low()));
                });

                auto const grid_snapped = std::ceil(raw_ideal / grid_.segment_width) * grid_.segment_width;
                auto const delta = grid_snapped - raw_ideal;

                nudged_offset = config.input_offset + (config.input_scale * delta);
                c_R += delta;
            }
        }

        auto const limit_profile = transition_factory_.make_limit(c_R, config.limit_width);

        if (limit_profile.band_low() < onset_profile.band_high())
        {
            return std::unexpected(builder_error_t::warp_overlap);
        }

        auto const expected_cap = c_R;
        auto const actual_cap = limit_profile.band_low()
            + (limit_profile.integral(limit_profile.band_high()) - limit_profile.integral(limit_profile.band_low()));
        auto const tolerance = 16.0 * std::numeric_limits<double>::epsilon()
            * (1.0 + std::abs(c_R) + (limit_profile.band_high() - limit_profile.center()));

        assert(std::abs(actual_cap - expected_cap) <= tolerance
            && "Precision loss: limit cap deviates from the expected crossing point.");

        auto final_chain = limit_warp_t{limit_profile, std::move(onset_chain)};
        return builder_result_t{std::move(final_chain), nudged_offset};
    }
};

template <typename real_t> struct quadratic_t
{
    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t { return x * x; }
    constexpr auto anchor() const noexcept -> std::optional<real_t> { return 0.0; }
};

template <typename real_t> struct sample_t
{
    real_t x;
    jet_t<real_t> y;

    constexpr auto near(sample_t const& src, real_t tolerance) const noexcept -> bool
    {
        return abs(rel_error(x, src.x)) < tolerance && abs(rel_error(primal(y), primal(src.y))) < tolerance
            && abs(rel_error(tangent(y), tangent(src.y))) < tolerance;
    }

    friend auto operator<<(std::ostream& out, sample_t const& src) -> std::ostream&
    {
        return out << src.x << " " << primal(src.y) << " " << tangent(src.y);
    }

    friend auto operator>>(std::istream& in, sample_t& src) -> std::istream&
    {
        return in >> src.x >> src.y.f >> src.y.df;
    }
};

TEST(signal_chain_test, integration)
{
    using real_t = float_t;

    auto const y0 = real_t{0.25};
    auto const output_scale = real_t{1.5};

    auto config = user_config_t<real_t>{
        .input_scale = 1.0,
        .input_offset = 0.0,
        .onset_center = 0.5,
        .onset_width = 0.25,
        .limit_target = 17.0,
        .limit_width = 0.5,
        .domain_low = 0.0,
        .domain_high = 256.0,
    };

    // auto grid = grid_params_t{.segment_width = 1.0, .global_tolerance = 1e-4};
    auto grid = grid_params_t<real_t>{.segment_width = std::ldexp(1.0, -10), .global_tolerance = 1e-4};

    auto const base_curve = quadratic_t<real_t>{};
    auto const affine_base = make_output_offset(y0, make_output_scale(output_scale, base_curve));

    auto const builder = signal_chain_builder_t<real_t, min_spline_transition_width<real_t>>{grid};
    auto const build_result = builder.build(config, base_curve.anchor(), affine_base);

    ASSERT_TRUE(build_result.has_value()) << to_string(build_result.error());

    auto const& output_chain = build_result->chain;

    auto const dx = static_cast<real_t>(0.001);
    auto const x_max = static_cast<real_t>(5.0) + dx * 3;

    auto const truth_path = std::filesystem::path{"signal_chain_integration_test_truth.txt"};
    auto const gen_truth = !std::filesystem::exists(truth_path);

    using sample_t = sample_t<real_t>;
    if (gen_truth)
    {
        auto truth_out = std::ofstream(truth_path);
        truth_out << std::setprecision(30);

        for (auto x = 0.0; x < x_max; x += dx)
        {
            auto const y = output_chain(jet_t{x, 1.0});
            auto const expected = sample_t{x, y};
            truth_out << expected << "\n";
        }

        return;
    }

    std::vector<sample_t> truth_table;
    auto truth_in = std::ifstream(truth_path);
    ASSERT_TRUE(truth_in.good() && !truth_in.bad());

    while (truth_in)
    {
        sample_t expected;
        truth_in >> expected;
        truth_table.push_back(expected);
    }

    // presume there are more than 1000 samples, but exactly how many varies.
    ASSERT_LT(1000, truth_table.size());

    constexpr auto tolerance = real_t{1e-10};

    auto truth_entry = std::begin(truth_table);
    for (auto x = 0.0; x < static_cast<real_t>(5.0) + dx * 3; x += dx)
    {
        auto const y = output_chain(jet_t{x, 1.0});

        auto const actual = sample_t{x, y};

        auto const expected = *truth_entry++;
        EXPECT_TRUE(expected.near(actual, tolerance));
    }
}

} // namespace crv::signal_chain
