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
#include <optional>
#include <string_view>
#include <utility>

namespace crv::signal_chain {

//
// math primitives & bounds
//

template <std::floating_point real_t> inline constexpr real_t min_spline_transition_width = real_t{1e-2};

//
// pure geometry (transitions)
//

template <typename real_t> struct smootherstep_integral_t
{
    // The integral of smootherstep, normalized to range over (t, y, y') in [(0, 0, 0), (1, 0.5, 1)]
    template <typename value_t> constexpr auto operator()(value_t t) const noexcept -> value_t
    {
        using scalar_t = scalar_type_t<value_t>;
        auto const rt = primal(t);

        if (rt <= scalar_t{0}) return value_t{0};
        if (rt >= scalar_t{1}) return value_t{0.5};

        // strictly compute using the primal scalar to avoid implicit jet evaluation
        auto const rt2 = rt * rt;
        auto const rt4 = rt2 * rt2;

        // f = t^6 - 3t^5 + 2.5t^4
        auto const f = rt4 * (rt2 - scalar_t{3} * rt + scalar_t{2.5});

        if constexpr (is_jet<value_t>)
        {
            // df = 6t^5 - 15t^4 + 10t^3
            auto const rt3 = rt2 * rt;
            auto const df = rt3 * ((scalar_t{6} * rt - scalar_t{15}) * rt + scalar_t{10});
            return value_t{f, df * tangent(t)};
        }
        else return f;
    }
};

//
// structural warps
//

template <typename real_t, typename prev_t, typename transition_t> class onset_warp_t
{
public:
    constexpr onset_warp_t(real_t center, real_t width, prev_t prev, transition_t transition)
        : start_{center - width / real_t{2}}, width_{width}, prev_{std::move(prev)}, transition_{std::move(transition)}
    {}

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        using scalar_t = scalar_type_t<value_t>;
        auto const rx = primal(x);

        if (rx <= start_) return prev_(value_t{0});
        if (rx >= start_ + width_) return prev_(x - start_ - width_ / scalar_t{2});

        auto const t = (x - start_) / width_;
        auto const w_int = transition_(t);
        return prev_(width_ * w_int);
    }

private:
    real_t start_;
    real_t width_;
    prev_t prev_;
    transition_t transition_;
};

template <typename real_t, typename prev_t, typename transition_t> class limit_warp_t
{
public:
    constexpr limit_warp_t(real_t start, real_t width, real_t blend, prev_t prev, transition_t transition)
        : start_{start}, width_{width}, blend_{blend}, prev_{std::move(prev)}, transition_{std::move(transition)}
    {}

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        using scalar_t = scalar_type_t<value_t>;
        auto const rx = primal(x);

        if (rx <= start_ || blend_ <= scalar_t{0}) return prev_(x);

        value_t warped_x;
        if (rx >= start_ + width_) { warped_x = value_t{start_ + width_ / scalar_t{2}}; }
        else
        {
            auto const t = (x - start_) / width_;
            auto const u = scalar_t{1} - t;
            auto const w_int = transition_(u);
            warped_x = start_ + width_ / scalar_t{2} - width_ * w_int;
        }

        auto const final_x = x + (warped_x - x) * scalar_t{blend_};
        return prev_(final_x);
    }

private:
    real_t start_;
    real_t width_;
    real_t blend_;
    prev_t prev_;
    transition_t transition_;
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

template <typename real_t, typename prev_t> constexpr auto make_input_affine(real_t scale, real_t offset, prev_t prev)
{
    return make_input_scale(scale, make_input_offset(offset, std::move(prev)));
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

template <typename real_t, typename prev_t> constexpr auto make_output_affine(real_t scale, real_t offset, prev_t prev)
{
    return make_output_offset(offset, make_output_scale(scale, std::move(prev)));
}

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

//
// signal chain builder
//

template <typename real_t> struct input_calibration_t
{
    real_t scale;
    real_t offset;
};

template <typename real_t> struct output_calibration_t
{
    real_t scale;
    std::optional<real_t> y0;
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
    invalid_domain,
    invalid_scale,
    invalid_floor,
    warp_overlap
};

constexpr auto to_string(builder_error_t error) noexcept -> std::string_view
{
    switch (error)
    {
        case builder_error_t::invalid_width: return "Transition width too small or negative.";
        case builder_error_t::invalid_domain:
            return "Domain low must be non-negative and strictly less than domain high.";
        case builder_error_t::invalid_scale: return "Scales must be strictly positive.";
        case builder_error_t::invalid_floor: return "Floor cannot be negative.";
        case builder_error_t::warp_overlap: return "Offset and limit transitions overlap.";
    }
    return "Unknown builder error.";
}

template <typename real_t, typename chain_t> struct builder_result_t
{
    chain_t chain;
    real_t nudged_input_offset;
};

template <typename real_t, real_t min_spline_transition_width, typename transition_t = smootherstep_integral_t<real_t>>
class signal_chain_builder_t
{
    bisect_lower_bound_t invert_;
    grid_params_t<real_t> grid_;
    transition_t transition_;

    // Chaining type inferences through strictly unevaluated decltype contexts
    template <typename curve_t>
    using out_stack_t
        = decltype(make_output_affine(real_t{}, real_t{}, make_normalize(real_t{}, std::declval<curve_t>())));

    template <typename curve_t> using onset_chain_t = onset_warp_t<real_t, out_stack_t<curve_t>, transition_t>;

    template <typename curve_t> using limit_chain_t = limit_warp_t<real_t, onset_chain_t<curve_t>, transition_t>;

    template <typename curve_t>
    using final_chain_t = decltype(make_input_affine(real_t{}, real_t{}, std::declval<limit_chain_t<curve_t>>()));

    template <typename curve_t>
    using expected_result_t = std::expected<builder_result_t<real_t, final_chain_t<curve_t>>, builder_error_t>;

public:
    constexpr signal_chain_builder_t(grid_params_t<real_t> grid, transition_t transition = {})
        : grid_{grid}, transition_{std::move(transition)}
    {}

    template <typename curve_t>
    [[nodiscard]] auto build(curve_t curve, domain_warp_config_t<real_t> const& warp,
        input_calibration_t<real_t> const& in, output_calibration_t<real_t> const& out) const
        -> expected_result_t<curve_t>
    {
        // Validation
        if (warp.onset_width <= min_spline_transition_width || warp.limit_width <= min_spline_transition_width)
            return std::unexpected(builder_error_t::invalid_width);

        if (warp.domain_low < real_t{0} || warp.domain_low >= warp.domain_high)
            return std::unexpected(builder_error_t::invalid_domain);

        if (in.scale <= real_t{0} || out.scale <= real_t{0}) return std::unexpected(builder_error_t::invalid_scale);

        if (out.y0 && *out.y0 < real_t{0}) return std::unexpected(builder_error_t::invalid_floor);

        // Base Curve & Output Transforms
        auto const f0 = curve(real_t{0});
        auto const anchor_y = curve.anchor().value_or(real_t{0});
        auto out_stack
            = make_output_affine(out.scale, out.y0.value_or(out.scale * f0), make_normalize(f0, std::move(curve)));
        auto onset_chain = onset_warp_t<real_t, decltype(out_stack), transition_t>{
            warp.onset_center, warp.onset_width, std::move(out_stack), transition_};

        // Graceful Roll-On Limit Placement
        auto const y_max = onset_chain(warp.domain_high);
        auto const opt_x_cap = invert_(warp.domain_low, warp.domain_high, warp.limit_target, onset_chain);

        real_t const x_cap = opt_x_cap.value_or(warp.domain_high);
        real_t const limit_start = x_cap - warp.limit_width / real_t{2};

        if (limit_start < warp.onset_center + warp.onset_width / real_t{2})
        {
            return std::unexpected(builder_error_t::warp_overlap);
        }

        real_t blend = real_t{1};
        if (warp.limit_target > y_max)
        {
            auto const d = warp.limit_target - y_max;
            auto const h = warp.limit_width / real_t{2};
            blend = std::max(real_t{0}, real_t{1} - d / h);
        }

        auto limit_chain = limit_warp_t<real_t, decltype(onset_chain), transition_t>{
            limit_start, warp.limit_width, blend, std::move(onset_chain), transition_};

        // Cusp Quantization & Knot Sinking
        auto const start_onset = warp.onset_center - warp.onset_width / real_t{2};
        auto const anchor_y = curve.anchor().value_or(real_t{0});

        real_t x_warp_in_target;
        if (anchor_y <= real_t{0}) { x_warp_in_target = start_onset; }
        else if (anchor_y >= warp.onset_width / real_t{2})
        {
            x_warp_in_target = anchor_y + start_onset + warp.onset_width / real_t{2};
        }
        else
        {
            // Analytically target the active transition integral
            auto const target_t = anchor_y / warp.onset_width;
            auto const found_t = invert_(real_t{0}, real_t{1}, target_t, transition_);
            x_warp_in_target = start_onset + found_t.value_or(real_t{0}) * warp.onset_width;
        }

        auto const x_raw = (x_warp_in_target - in.offset) / in.scale;
        real_t nudged_offset;

        if (x_raw > warp.domain_high) { nudged_offset = in.offset; }
        else
        {
            auto const x_q = std::ceil(x_raw / grid_.segment_width) * grid_.segment_width;
            nudged_offset = x_warp_in_target - x_q * in.scale;
        }

        // Final Assembly
        auto final_chain = make_input_affine(in.scale, nudged_offset, std::move(limit_chain));

        return builder_result_t<real_t, decltype(final_chain)>{std::move(final_chain), nudged_offset};
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
    auto const& result = *build_result;

    auto const dx = static_cast<real_t>(0.1);
    for (auto x = real_t{0}; x < real_t{5}; x += dx)
    {
        auto const y = result.chain(jet_t{x, real_t{1}});
        std::cout << sample_t{x, y} << std::endl;
        ASSERT_TRUE(std::isfinite(primal(y)) && std::isfinite(tangent(y))) << "non-finite at x = " << x;
    }
}

} // namespace crv::signal_chain
