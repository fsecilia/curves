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

template <typename real_t, typename prev_t>
using input_affine_t = input_scale_t<real_t, input_offset_t<real_t, prev_t>>;

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

template <typename real_t, typename prev_t>
using output_affine_t = output_offset_t<real_t, output_scale_t<real_t, prev_t>>;

template <typename real_t, typename prev_t> struct normalize_t
{
    real_t origin;
    prev_t prev;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return prev(x) - origin;
    }
};

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

template <typename real_t, real_t min_width>
constexpr auto validate_params(domain_warp_config_t<real_t> const& warp, input_calibration_t<real_t> const& in,
    output_calibration_t<real_t> const& out) noexcept -> std::optional<builder_error_t>
{
    if (warp.onset_width <= min_width || warp.limit_width <= min_width) return builder_error_t::invalid_width;
    if (warp.domain_low < real_t{0} || warp.domain_low >= warp.domain_high) return builder_error_t::invalid_domain;
    if (in.scale <= real_t{0} || out.scale <= real_t{0}) return builder_error_t::invalid_scale;
    if (out.y0 && *out.y0 < real_t{0}) return builder_error_t::invalid_floor;
    return std::nullopt;
}

template <typename real_t, typename transition_t, typename invert_t = bisect_lower_bound_t> struct cusp_quantizer_t
{
    invert_t invert;

    [[nodiscard]] constexpr auto operator()(std::optional<real_t> anchor_y, domain_warp_config_t<real_t> const& warp,
        input_calibration_t<real_t> const& in, grid_params_t<real_t> const& grid, transition_t const& transition) const
        -> real_t
    {
        auto const start_onset = warp.onset_center - warp.onset_width / real_t{2};
        auto const anchor_val = anchor_y.value_or(real_t{0});

        real_t x_warp_in_target; // onset-input (curve space) where the anchor lands
        if (anchor_val <= real_t{0}) x_warp_in_target = start_onset;
        else if (anchor_val >= warp.onset_width / real_t{2})
        {
            x_warp_in_target = anchor_val + start_onset + warp.onset_width / real_t{2};
        }
        else
        {
            auto const target_t = anchor_val / warp.onset_width;
            auto const found_t = invert(real_t{0}, real_t{1}, target_t, transition);
            x_warp_in_target = start_onset + found_t.value_or(real_t{0}) * warp.onset_width;
        }

        auto const x_raw = (x_warp_in_target - in.offset) / in.scale;
        if (x_raw > warp.domain_high) return in.offset;

        auto const x_q = std::ceil(x_raw / grid.segment_width) * grid.segment_width; // forward, never negative
        return x_warp_in_target - x_q * in.scale; // land the cusp on x_q
    }
};

template <typename real_t, typename invert_t = bisect_lower_bound_t> struct limit_locator_t
{
    invert_t invert;

    template <typename chain_t>
    [[nodiscard]] constexpr auto operator()(domain_warp_config_t<real_t> const& warp, real_t onset_in_low,
        real_t onset_in_high, chain_t const& onset_chain) const -> std::expected<real_t, builder_error_t>
    {
        auto const opt_x_cap = invert(onset_in_low, onset_in_high, warp.limit_target, onset_chain);
        auto const x_cap = opt_x_cap.value_or(onset_in_high);
        auto const limit_start = x_cap - warp.limit_width / real_t{2};

        if (limit_start < warp.onset_center + warp.onset_width / real_t{2})
        {
            return std::unexpected(builder_error_t::warp_overlap);
        }

        return limit_start;
    }
};

template <typename real_t, real_t min_spline_transition_width, typename transition_t = smootherstep_integral_t<real_t>,
    typename quantizer_t = cusp_quantizer_t<real_t, transition_t>, typename locator_t = limit_locator_t<real_t>>
class signal_chain_builder_t
{
    grid_params_t<real_t> grid_;
    transition_t transition_;
    quantizer_t quantize_;
    locator_t locate_limit_;

    template <typename curve_t> using out_stack_t = output_affine_t<real_t, normalize_t<real_t, curve_t>>;
    template <typename curve_t> using onset_chain_t = onset_warp_t<real_t, out_stack_t<curve_t>, transition_t>;
    template <typename curve_t> using limit_chain_t = limit_warp_t<real_t, onset_chain_t<curve_t>, transition_t>;
    template <typename curve_t> using final_chain_t = input_affine_t<real_t, limit_chain_t<curve_t>>;
    template <typename curve_t> using result_t = builder_result_t<real_t, final_chain_t<curve_t>>;

public:
    constexpr signal_chain_builder_t(grid_params_t<real_t> grid, transition_t transition = {},
        quantizer_t quantize = {}, locator_t locate_limit = {})
        : grid_{grid}, transition_{std::move(transition)}, quantize_{std::move(quantize)},
          locate_limit_{std::move(locate_limit)}
    {}

    template <typename curve_t>
    [[nodiscard]] auto build(curve_t curve, domain_warp_config_t<real_t> const& warp,
        input_calibration_t<real_t> const& in, output_calibration_t<real_t> const& out) const
        -> std::expected<result_t<curve_t>, builder_error_t>
    {
        //
        // validation
        //

        if (auto const err = validate_params<real_t, min_spline_transition_width>(warp, in, out))
        {
            return std::unexpected(*err);
        }

        // cache values from curve before move.
        auto const f0 = curve(real_t{0});
        auto const anchor_y = curve.anchor();

        //
        // cusp quantization
        //

        real_t const nudged_offset = quantize_(anchor_y, warp, in, grid_, transition_);

        //
        // transforms
        //

        auto out_stack = output_offset_t{
            .offset = out.y0.value_or(out.scale * f0),
            .prev = output_scale_t{.scale = out.scale, .prev = normalize_t{f0, std::move(curve)}},
        };
        auto onset_chain = onset_warp_t{warp.onset_center, warp.onset_width, std::move(out_stack), transition_};

        // Map the visible domain into curve space with the affine the chain will use. domain_low/high are final-input
        // (the spline domain); onset_center/width and the limit live in curve space. The input affine bridges them, so
        // the bounds must pass through it before they reach onset_chain. This way, domain_high does not appear in two
        // different frames.
        auto const onset_in_low = warp.domain_low * in.scale + nudged_offset;
        auto const onset_in_high = warp.domain_high * in.scale + nudged_offset;

        //
        // limit placement
        //

        // one uniform rule: Position rides the curve via the inverse; with the line above the curve there is no
        // intersection and we default to the domain edge, so position stays continuous through y_max there.
        // Flat-topped / mid-plateau curves jump at their flat runs; that is inherent to a lower-bound
        // inverse and is left to a temporal (EMA) chase post-MVP.
        auto const limit_start_res = locate_limit_(warp, onset_in_low, onset_in_high, onset_chain);
        if (!limit_start_res.has_value()) return std::unexpected(limit_start_res.error());

        auto const limit_start = *limit_start_res;

        //
        // engagement
        //

        // strength: slope-independent linear roll-on in y. d(blend)/d(limit_target) = -1/window_y is constant, same for
        // steep or tall. Essential even when rising: without it the edge-default clamp would pull the curve's top down
        // while the line merely hovers above.
        //
        // Roll-on window is the box's height. In the isotropic curve space one x-unit equals one y-unit, so
        // limit_width/2 is a legitimate y-distance; no separate parameter, no axis conversion. Slope-free and
        // height-free here: d(blend)/d(limit_target) = -2/limit_width, constant.
        real_t blend = real_t{1};
        auto const y_max = onset_chain(onset_in_high);
        if (warp.limit_target > y_max)
        {
            auto const dy = warp.limit_target - y_max; // gap above the curve's top, data-y
            auto const h = warp.limit_width / real_t{2}; // box height in the isotropic curve space
            blend = std::max(real_t{0}, real_t{1} - dy / h);
        }

        auto limit_chain = limit_warp_t{limit_start, warp.limit_width, blend, std::move(onset_chain), transition_};

        //
        // final assembly
        //

        auto final_chain = input_scale_t{
            .scale = in.scale,
            .prev = input_offset_t{.offset = nudged_offset, .prev = std::move(limit_chain)},
        };
        return result_t<curve_t>{std::move(final_chain), nudged_offset};
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
        return out << src.x << "\t" << primal(src.y) << "\t" << tangent(src.y);
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

    auto& out = std::cout;
    auto const dx = static_cast<real_t>(0.1);
    for (auto x = real_t{0}; x < real_t{5}; x += dx)
    {
        auto const y = result.chain(jet_t{x, real_t{1}});
        out << sample_t{x, y} << std::endl;
        ASSERT_TRUE(std::isfinite(primal(y)) && std::isfinite(tangent(y))) << "non-finite at x = " << x;
    }
}

} // namespace crv::signal_chain
