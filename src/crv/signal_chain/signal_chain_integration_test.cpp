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
// structural warps
//

template <typename prev_t, typename transition_t> class onset_warp_t
{
public:
    using real_t = typename transition_t::real_t;

    constexpr onset_warp_t(transition_t transition, prev_t prev)
        : transition_{std::move(transition)}, prev_{std::move(prev)}
    {
        lag_ = transition_.half_width();
    }

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        auto const rx = primal(x);

        if (rx <= transition_.band_low()) return value_t{};
        if (rx < transition_.band_high()) return prev_(transition_.warp(x, real_t{0}));

        return prev_(x - lag_);
    }

    auto lag() const noexcept -> real_t { return lag_; }

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
        cap_end_ = cap_start_ + transition_.half_width();
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
// affine adapters
//

template <typename real_t, typename prev_t> struct input_scale_t
{
    real_t scale;
    prev_t prev;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return prev(x * scale);
    }
};

template <typename real_t, typename prev_t> struct input_offset_t
{
    real_t offset;
    prev_t prev;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return prev(x + offset);
    }
};

template <typename real_t, typename prev_t> constexpr auto make_input_scale(real_t scale, prev_t prev)
{
    return input_scale_t<real_t, std::remove_cvref_t<prev_t>>{scale, std::move(prev)};
}

template <typename real_t, typename prev_t> constexpr auto make_input_offset(real_t offset, prev_t prev)
{
    return input_offset_t<real_t, std::remove_cvref_t<prev_t>>{offset, std::move(prev)};
}

// scale * x + offset, nested so the convention can't be flipped at the call site
template <typename real_t, typename prev_t> constexpr auto make_input_affine(real_t scale, real_t offset, prev_t prev)
{
    return make_input_scale(scale, make_input_offset(offset, std::move(prev)));
}

// scale * f + offset, matching the existing output_* nesting
template <typename real_t, typename prev_t> constexpr auto make_output_affine(real_t scale, real_t offset, prev_t prev)
{
    return make_output_offset(offset, make_output_scale(scale, std::move(prev)));
}

// origin normalization: subtract f at the curve start so the shape begins at 0
template <typename real_t, typename prev_t> struct normalize_t
{
    real_t origin;
    prev_t prev;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return prev(x) - origin;
    }
};

template <typename real_t, typename prev_t> constexpr auto make_normalize(real_t origin, prev_t prev)
{
    return normalize_t<real_t, std::remove_cvref_t<prev_t>>{origin, std::move(prev)};
}

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

template <typename real_t> struct input_calibration_t
{
    real_t scale;
    real_t offset; // seed for the grid-alignment solve
};

template <typename real_t> struct output_calibration_t
{
    real_t scale;
    std::optional<real_t> y0; // unset: natural floor f(0); set (>= 0): pin floor to y0
};

template <typename real_t> struct domain_warp_config_t
{
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
    limit_not_reached,
    invalid_floor
};

constexpr auto to_string(builder_error_t error) noexcept -> std::string_view
{
    switch (error)
    {
        case builder_error_t::invalid_width: return "Transition width too small or negative.";
        case builder_error_t::warp_overlap: return "Offset and limit transitions overlap.";
        case builder_error_t::excessive_curvature: return "Transition violates hermite cubic bounds.";
        case builder_error_t::limit_not_reached: return "Base curve never reaches limit target.";
        case builder_error_t::invalid_floor: return "Floor cannot be negative.";
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

    template <typename curve_t>
    using chain_t = input_scale_t<real_t,
        input_offset_t<real_t,
            limit_warp_t<onset_warp_t<output_offset_t<real_t, output_scale_t<real_t, normalize_t<real_t, curve_t>>>,
                             transition_t>,
                transition_t>>>;

    using grid_params_t = grid_params_t<real_t>;

    transition_factory_t transition_factory_;
    bisect_lower_bound_t invert_;
    grid_params_t grid_;

    // TODO(roll-on): a flat region pops as a function of limit_target; the soft roll-on lands in the dedicated
    // pass. For now place at the leftmost crossing (correct while the curve is increasing) and fail loud when it
    // never reaches the target.
    template <typename onset_chain_t>
    [[nodiscard]] auto place_limit(onset_chain_t const& onset_chain, domain_warp_config_t<real_t> const& warp) const
        -> std::optional<real_t>
    {
        auto const limit_half_width = transition_factory_.make_limit(real_t{0}, warp.limit_width).half_width();
        auto const search_high = warp.domain_high + limit_half_width - onset_chain.lag();
        return invert_(warp.domain_low, search_high, warp.limit_target, onset_chain);
    }

    [[nodiscard]] auto solve_input_offset(input_calibration_t<real_t> const& in, transition_t const&, real_t,
        domain_warp_config_t<real_t> const&) const -> real_t
    {
        // STUB(nudge): anchor->grid alignment is deferred to the dedicated pass. The offset passes through
        // unaligned. The chain is correct, it is simply not knot-aligned. A green test here does NOT mean
        // alignment works — there is intentionally no alignment assertion yet.
        return in.offset;
    }

public:
    template <typename prev_t>
    using builder_result_t = builder_result_t<real_t, limit_warp_t<onset_warp_t<prev_t, transition_t>, transition_t>>;

    constexpr signal_chain_builder_t(grid_params_t grid) : grid_{grid} {}

    template <typename curve_t>
    [[nodiscard]] auto build(curve_t curve, domain_warp_config_t<real_t> const& warp,
        input_calibration_t<real_t> const& in, output_calibration_t<real_t> const& out) const
        -> std::expected<chain_t<curve_t>, builder_error_t> // chain_t = the spelled-out nested stage type
    {
        // validate
        if (warp.onset_width <= min_spline_transition_width || warp.limit_width <= min_spline_transition_width)
        {
            return std::unexpected(builder_error_t::invalid_width);
        }

        if (out.y0 && *out.y0 < real_t{0}) return std::unexpected(builder_error_t::invalid_floor);

        // transitions, each built once
        auto const onset = transition_factory_.make_onset(warp.onset_center, warp.onset_width);
        auto const limit_shape = transition_factory_.make_limit(real_t{0}, warp.limit_width); // k is center-independent

        auto const max_d4
            = hermite_cubic_policy_t<real_t>::max_tolerable_d4(grid_.segment_width, grid_.global_tolerance);
        if (onset.peak_d4() > max_d4 || limit_shape.peak_d4() > max_d4)
        {
            return std::unexpected(builder_error_t::excessive_curvature);
        }

        // auto const anchor = curve.anchor();
        auto const f0 = curve(real_t{0}); // scalar path; quadratic gives 0 and the normalize is a no-op

        // output stack: scale * (f - f0) + b
        auto out_stack
            = make_output_affine(out.scale, out.y0.value_or(out.scale * f0), make_normalize(f0, std::move(curve)));

        // domain warps over the output stack
        // TODO(roll-on): onset/limit order and limit placement are provisional, pinned in the dedicated pass
        auto onset_chain = onset_warp_t{onset, std::move(out_stack)};

        auto const opt_center = place_limit(onset_chain, warp);
        if (!opt_center) return std::unexpected(builder_error_t::limit_not_reached);
        auto const limit_center = *opt_center;

        auto const limit = transition_factory_.make_limit(limit_center, warp.limit_width);
        if (limit.band_low() < onset.band_high()) return std::unexpected(builder_error_t::warp_overlap);

        // input affine over the warps; offset solved so the anchor lands on a grid node
        auto warped = limit_warp_t{limit, std::move(onset_chain)};
        auto const offset = solve_input_offset(in, onset, limit_center, warp);
        return make_input_affine(in.scale, offset, std::move(warped));
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

TEST(signal_chain_test, assembles_and_evaluates)
{
    using real_t = float_t;

    auto const grid = grid_params_t<real_t>{.segment_width = std::ldexp(1.0, -10), .global_tolerance = 1e-4};
    auto const builder = signal_chain_builder_t<real_t, min_spline_transition_width<real_t>>{grid};

    auto const build_result = builder.build(quadratic_t<real_t>{},
        domain_warp_config_t<real_t>{.onset_center = 0.5,
            .onset_width = 0.25,
            .limit_target = 17.0,
            .limit_width = 0.5,
            .domain_low = 0.0,
            .domain_high = 256.0},
        input_calibration_t<real_t>{.scale = 1.0, .offset = 0.0},
        output_calibration_t<real_t>{.scale = 1.5, .y0 = 0.25});

    ASSERT_TRUE(build_result.has_value()) << to_string(build_result.error());
    auto const& chain = *build_result;

    auto const dx = static_cast<real_t>(0.1);
    for (auto x = real_t{0}; x < real_t{5}; x += dx)
    {
        auto const y = chain(jet_t{x, real_t{1}});
        std::cout << sample_t{x, y} << std::endl;
        ASSERT_TRUE(std::isfinite(primal(y)) && std::isfinite(tangent(y))) << "non-finite at x = " << x;
    }
}

} // namespace crv::signal_chain
