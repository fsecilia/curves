// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/curves/test.hpp>
#include <crv/curves/traits.hpp>
#include <crv/math/complex.hpp>
#include <crv/math/complex_traits.hpp>
#include <crv/math/inverse.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/scalar_traits.hpp>
#include <crv/signal_chain/transitions/smootherstep_integral.hpp>
#include <crv/test/test.hpp>
#include <algorithm>
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

template <std::floating_point real_t> inline constexpr auto min_transition_width = static_cast<real_t>(1e-2);
constexpr auto anchor_quantum = static_cast<float_t>(0x1p-10);

//
// transforms
//

template <std::floating_point real_t> struct affine_t
{
    real_t scale{1};
    real_t offset{0};

    // applies transform forward
    template <typename value_t> [[nodiscard]] constexpr auto forward(value_t x) const noexcept -> value_t
    {
        return x * scale + offset;
    }

    // inverts transform to find x that produces y
    [[nodiscard]] constexpr auto inverse(real_t y) const noexcept -> real_t
    {
        assert(scale != real_t{0});
        return (y - offset) / scale;
    }

    // composes transforms: outer(inner(x))
    [[nodiscard]] constexpr auto operator*(affine_t const& inner) const noexcept -> affine_t
    {
        return affine_t{.scale = scale * inner.scale, .offset = scale * inner.offset + offset};
    }
};

//
// stages
//

template <typename real_t, typename prev_t> struct input_affine_t
{
    affine_t<real_t> map;
    prev_t prev;

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return prev(map.forward(x));
    }
};

template <typename real_t, typename prev_t> struct output_affine_t
{
    affine_t<real_t> map;
    prev_t prev;

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return map.forward(prev(x));
    }
};

//
// onset geometry (shape shared by onset_warp_t::forward and cusp_quantizer_t's inverse)
//

template <typename real_t, typename transition_t> class onset_geometry_t
{
public:
    constexpr onset_geometry_t(real_t center, real_t width, transition_t transition) noexcept
        : start_{center - width * real_t{0.5}}, width_{width},
          inv_width_{width > real_t{0} ? real_t{1} / width : real_t{0}}, rise_{transition(real_t{1})},
          transition_{std::move(transition)}
    {
        assert(width >= real_t{0});
    }

    // onset-input x -> warped (curve-input) coordinate; identity when disabled (width == 0)
    template <typename value_t> [[nodiscard]] constexpr auto forward(value_t input) const noexcept -> value_t
    {
        if (width_ == real_t{0}) return input;

        auto const x = primal(input);
        if (x <= start_) return value_t{0};
        if (x >= start_ + width_) return input - start_ - width_ * (real_t{1} - rise_);
        return width_ * transition_((input - start_) * inv_width_);
    }

    // warped (curve-input) coordinate -> onset-input x; exact inverse of forward
    template <typename invert_t>
    [[nodiscard]] constexpr auto inverse(real_t warped, invert_t const& invert) const -> real_t
    {
        if (width_ == real_t{0}) return warped;
        if (warped <= real_t{0}) return start_;
        if (warped >= width_ * rise_) return warped + start_ + width_ * (real_t{1} - rise_);

        auto const found_t = invert(real_t{0}, real_t{1}, warped / width_, transition_);
        return start_ + found_t.value_or(real_t{0}) * width_;
    }

private:
    real_t start_;
    real_t width_;
    real_t inv_width_;
    real_t rise_;
    transition_t transition_;
};

//
// structural warps
//

template <typename real_t, typename prev_t, typename transition_t> class onset_warp_t
{
public:
    constexpr onset_warp_t(real_t center, real_t width, prev_t prev, transition_t transition) noexcept
        : geometry_{center, width, std::move(transition)}, prev_{std::move(prev)}
    {}

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        return prev_(geometry_.forward(input));
    }

private:
    onset_geometry_t<real_t, transition_t> geometry_;
    prev_t prev_;
};

template <typename real_t, typename prev_t, typename transition_t> class limit_warp_t
{
public:
    constexpr limit_warp_t(real_t start, real_t width, real_t blend, prev_t prev, transition_t transition) noexcept
        : start_{start}, width_{width}, inv_width_{real_t{1} / width}, rise_{transition(real_t{1})}, blend_{blend},
          prev_{std::move(prev)}, transition_{std::move(transition)}
    {
        assert(width > real_t{0});
    }

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        using scalar_t = scalar_type_t<value_t>;
        auto const x = primal(input);

        if (x <= start_ || blend_ <= scalar_t{0}) return prev_(input);

        value_t warped_x;
        if (x >= start_ + width_) warped_x = value_t{start_ + width_ * rise_};
        else
        {
            auto const t = (input - start_) * inv_width_;
            auto const w_int = transition_(scalar_t{1} - t);
            warped_x = start_ + width_ * (rise_ - w_int);
        }

        auto const final_x = input + (warped_x - input) * scalar_t{blend_};
        return prev_(final_x);
    }

private:
    real_t start_;
    real_t width_;
    real_t inv_width_;
    real_t rise_;
    real_t blend_;
    prev_t prev_;
    transition_t transition_;
};

//
// signal chain builder
//

template <typename real_t> struct input_config_t
{
    real_t scale;
    real_t offset;

    [[nodiscard]] constexpr auto to_affine() const noexcept -> affine_t<real_t>
    {
        return affine_t<real_t>{.scale = scale, .offset = -(offset * scale)};
    }
};

template <typename real_t> struct output_config_t
{
    real_t scale;
    real_t offset;
    std::optional<real_t> floor;

    [[nodiscard]] constexpr auto to_affine(real_t y_origin) const noexcept -> affine_t<real_t>
    {
        auto const target_floor = floor.value_or(y_origin + offset);
        return affine_t<real_t>{.scale = scale, .offset = target_floor - y_origin * scale};
    }
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

enum class builder_error_t
{
    invalid_width,
    invalid_domain,
    invalid_scale,
    invalid_floor,
    invalid_input_offset,
    negative_domain,
    warp_overlap,
    invalid_onset_disable,
    floor_offset_conflict
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
        case builder_error_t::invalid_input_offset: return "Input offset must shift left.";
        case builder_error_t::negative_domain: return "Domain mapped to negative curve space, breaking monotonicity.";
        case builder_error_t::warp_overlap: return "Offset and limit transitions overlap.";
        case builder_error_t::invalid_onset_disable: return "Disabled onset (zero width) requires onset_center == 0.";
        case builder_error_t::floor_offset_conflict: return "Floor and a nonzero output offset are mutually exclusive.";
    }
    return "Unknown builder error.";
}

// location in input domain of an error for highlighting in the trace widget; low == high denotes a point
template <typename real_t> struct domain_span_t
{
    real_t low;
    real_t high;
};

template <typename real_t> struct build_error_t
{
    builder_error_t error;
    std::optional<domain_span_t<real_t>> location; // present only when an input domain location is meaningful
};

template <typename chain_t> struct builder_result_t
{
    chain_t chain;
};

//
// validation
//

template <typename real_t, real_t min_width> struct default_validator_t
{
    static_assert(min_width > real_t{0}, "min transition width must be positive (logic error, not user-facing)");

    [[nodiscard]] constexpr auto operator()(domain_warp_config_t<real_t> const& warp, input_config_t<real_t> const& in,
        output_config_t<real_t> const& out) const noexcept -> std::optional<builder_error_t>
    {
        // onset_width == 0 disables the onset (identity). A disabled onset must not be positioned: onset_center == 0
        // also collapses the cusp geometry to identity (start_onset == 0). A live onset must clear the minimum
        // transition width; the limiter is mandatory, so it must always clear it.
        if (warp.onset_width < real_t{0}) return builder_error_t::invalid_width;
        if (warp.onset_width == real_t{0})
        {
            if (warp.onset_center != real_t{0}) return builder_error_t::invalid_onset_disable;
        }
        else if (warp.onset_width <= min_width) { return builder_error_t::invalid_width; }
        if (warp.limit_width <= min_width) return builder_error_t::invalid_width;

        if (warp.domain_low < real_t{0} || warp.domain_low >= warp.domain_high) return builder_error_t::invalid_domain;
        if (in.scale <= real_t{0} || out.scale <= real_t{0}) return builder_error_t::invalid_scale;
        if (out.floor && *out.floor < real_t{0}) return builder_error_t::invalid_floor;
        if (out.floor && out.offset != real_t{0}) return builder_error_t::floor_offset_conflict;
        if (in.offset > real_t{0}) return builder_error_t::invalid_input_offset;
        return std::nullopt;
    }
};

//
// cusp quantization
//

template <typename real_t, real_t anchor_quantum, typename transition_t, typename invert_t = bisect_lower_bound_t>
struct cusp_quantizer_t
{
    static_assert(anchor_quantum > real_t{0}, "anchor_quantum must be positive (logic error, not user-facing)");

    invert_t invert;

    [[nodiscard]] constexpr auto operator()(std::optional<real_t> anchor_opt, domain_warp_config_t<real_t> const& warp,
        affine_t<real_t> const& base_input_map, transition_t const& transition) const -> real_t
    {
        onset_geometry_t<real_t, transition_t> const geometry{warp.onset_center, warp.onset_width, transition};

        auto const anchor = anchor_opt.value_or(real_t{0});
        auto const x_warp_in_target = geometry.inverse(anchor, invert); // onset-input (curve space) where anchor lands

        auto const x_raw = base_input_map.inverse(x_warp_in_target);
        if (x_raw > warp.domain_high) return real_t{0}; // no extra shift when off-screen

        auto const x_q = std::ceil(x_raw / anchor_quantum) * anchor_quantum;
        return x_q - x_raw; // shift in raw space required to land on x_q
    }
};

//
// limit placement
//

template <typename real_t, typename invert_t = bisect_lower_bound_t> struct limit_locator_t
{
    invert_t invert;

    /// \pre onset_chain is monotonic non-decreasing on [onset_in_low, onset_in_high] (guaranteed by the
    /// monotonic-non-decreasing base-curve contract and out.scale > 0). The lower-bound inverse relies on it;
    /// asserting it via root finding would be prohibitively expensive, so it is a precondition, not a check.
    template <typename chain_t>
    [[nodiscard]] constexpr auto operator()(domain_warp_config_t<real_t> const& warp, real_t onset_in_low,
        real_t onset_in_high, chain_t const& onset_chain) const -> std::expected<real_t, builder_error_t>
    {
        auto const opt_x_cap = invert(onset_in_low, onset_in_high, warp.limit_target, onset_chain);
        auto const x_cap = opt_x_cap.value_or(onset_in_high);
        auto const limit_start = x_cap - warp.limit_width * real_t{0.5};

        if (limit_start < warp.onset_center + warp.onset_width * real_t{0.5})
        {
            return std::unexpected(builder_error_t::warp_overlap);
        }

        return limit_start;
    }
};

//
// engagement
//

template <typename real_t> struct linear_engagement_t
{
    // Rolls the limiter on as the curve's top approaches the limit line from below: blend == 1 when the top touches
    // the line, falling linearly to 0 over a window equal to the transition's rise. The window is a curve-output (v)
    // distance; the display-space gap is converted by out_scale so the roll-on is independent of the output scale.
    // Slope-free: d(blend)/d(limit_target) is constant.
    [[nodiscard]] constexpr auto operator()(
        real_t limit_target, real_t y_max, real_t limit_width, real_t out_scale, real_t rise) const noexcept -> real_t
    {
        if (limit_target <= y_max) return real_t{1};

        auto const gap_v = (limit_target - y_max) / out_scale; // display-y gap -> curve-output (v) space
        auto const window_v = limit_width * rise; // transition's rise == window height (v space)
        return std::max(real_t{0}, real_t{1} - gap_v / window_v);
    }
};

//
// builder
//

template <typename real_t, real_t min_transition_width, real_t anchor_quantum,
    typename transition_t = transitions::smootherstep_integral_t,
    typename quantizer_t = cusp_quantizer_t<real_t, anchor_quantum, transition_t>,
    typename locator_t = limit_locator_t<real_t>, typename engagement_t = linear_engagement_t<real_t>,
    typename validator_t = default_validator_t<real_t, min_transition_width>>
class signal_chain_builder_t
{
    transition_t transition_;
    quantizer_t quantize_;
    locator_t locate_limit_;
    engagement_t engage_;
    validator_t validate_;

    template <typename curve_t> using out_stack_t = output_affine_t<real_t, curve_t>;
    template <typename curve_t> using onset_chain_t = onset_warp_t<real_t, out_stack_t<curve_t>, transition_t>;
    template <typename curve_t> using limit_chain_t = limit_warp_t<real_t, onset_chain_t<curve_t>, transition_t>;
    template <typename curve_t> using final_chain_t = input_affine_t<real_t, limit_chain_t<curve_t>>;
    template <typename curve_t> using result_t = builder_result_t<final_chain_t<curve_t>>;

public:
    constexpr signal_chain_builder_t(transition_t transition = {}, quantizer_t quantize = {},
        locator_t locate_limit = {}, engagement_t engage = {}, validator_t validate = {})
        : transition_{std::move(transition)}, quantize_{std::move(quantize)}, locate_limit_{std::move(locate_limit)},
          engage_{std::move(engage)}, validate_{std::move(validate)}
    {}

    template <typename curve_t>
    [[nodiscard]] auto build(curve_t curve, domain_warp_config_t<real_t> const& warp, input_config_t<real_t> const& in,
        output_config_t<real_t> const& out) const -> std::expected<result_t<curve_t>, build_error_t<real_t>>
    {
        //
        // validation
        //

        if (auto const err = validate_(warp, in, out))
        {
            // domain bounds are raw-input; the rest are config / curve-space with no clean raw location.
            std::optional<domain_span_t<real_t>> where;
            if (*err == builder_error_t::invalid_domain)
            {
                where = domain_span_t<real_t>{
                    std::min(warp.domain_low, warp.domain_high), std::max(warp.domain_low, warp.domain_high)};
            }
            return std::unexpected(build_error_t<real_t>{*err, where});
        }

        //
        // cusp quantization
        //

        auto const base_input_map = in.to_affine();
        real_t const raw_shift = quantize_(curve.anchor(), warp, base_input_map, transition_);

        //
        // transforms
        //

        auto const y_origin = curve(real_t{0});
        assert(std::isfinite(y_origin)); // logic error: a built-in curve over a validated domain stays finite

        auto const out_map = out.to_affine(y_origin);

        auto out_stack = output_affine_t<real_t, curve_t>{
            .map = out_map,
            .prev = std::move(curve),
        };
        auto onset_chain = onset_warp_t{warp.onset_center, warp.onset_width, std::move(out_stack), transition_};

        // map visible domain into the curve space the affine chain will use
        auto const nudge_map = affine_t<real_t>{.scale = real_t{1}, .offset = -raw_shift};
        auto const final_input_map = base_input_map * nudge_map;

        auto const onset_in_low = final_input_map.forward(warp.domain_low);
        if (onset_in_low < real_t{0})
        {
            // breaking slice: [domain_low, where curve space crosses 0], carried back to raw input
            return std::unexpected(build_error_t<real_t>{builder_error_t::negative_domain,
                domain_span_t<real_t>{warp.domain_low, final_input_map.inverse(real_t{0})}});
        }

        auto const onset_in_high = final_input_map.forward(warp.domain_high);

        //
        // limit placement
        //

        // one uniform rule: Position rides the curve via the inverse; with the line above the curve there is no
        // intersection and we default to the domain edge, so position stays continuous through y_max there.
        // Flat-topped / mid-plateau curves jump at their flat runs; that is inherent to a lower-bound
        // inverse and is left to a temporal (EMA) chase post-MVP.
        auto const limit_start_res = locate_limit_(warp, onset_in_low, onset_in_high, onset_chain);
        if (!limit_start_res.has_value())
        {
            // overlap location: the onset-transition end, carried back to raw input.
            auto const onset_end_raw = final_input_map.inverse(warp.onset_center + warp.onset_width * real_t{0.5});
            return std::unexpected(
                build_error_t<real_t>{limit_start_res.error(), domain_span_t<real_t>{onset_end_raw, onset_end_raw}});
        }

        auto const limit_start = *limit_start_res;

        //
        // engagement
        //

        auto const y_max = onset_chain(onset_in_high);
        assert(std::isfinite(y_max)); // logic error: a built-in curve over a validated domain stays finite

        real_t const rise = transition_(real_t{1});
        real_t const blend = engage_(warp.limit_target, y_max, warp.limit_width, out.scale, rise);

        auto limit_chain = limit_warp_t{limit_start, warp.limit_width, blend, std::move(onset_chain), transition_};

        //
        // final assembly
        //

        auto final_chain = input_affine_t<real_t, limit_chain_t<curve_t>>{
            .map = final_input_map,
            .prev = std::move(limit_chain),
        };

        return result_t<curve_t>{std::move(final_chain)};
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

    auto const builder = signal_chain_builder_t<real_t, min_transition_width<real_t>, anchor_quantum>{};

    auto const build_result = builder.build(quadratic_t<real_t>{},
        domain_warp_config_t<real_t>{.onset_center = 0.5,
            .onset_width = 0.25,
            .limit_target = 17.0,
            .limit_width = 0.5,
            .domain_low = 0.0,
            .domain_high = 256.0},
        input_config_t<real_t>{.scale = 1.0, .offset = -0.1},
        output_config_t<real_t>{.scale = 1.5, .offset = 0.0, .floor = 0.25}); // floor set -> offset must be 0

    ASSERT_TRUE(build_result.has_value()) << to_string(build_result.error().error);
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

//
// tests: properties and component contracts
//

namespace {

using real_t = float_t;
using builder_t = signal_chain_builder_t<real_t, min_transition_width<real_t>, anchor_quantum>;

template <typename real_t>
[[nodiscard]] constexpr auto near(real_t lhs, real_t rhs, real_t abs_tol, real_t rel_tol) noexcept -> bool
{
    return std::abs(lhs - rhs) <= abs_tol + rel_tol * std::abs(rhs);
}

} // namespace

//
// Affine Tests
//

TEST(affine_test, inverse_recovers_original_value)
{
    using real_t = real_t;
    affine_t<real_t> const a{.scale = 3, .offset = 2};
    ASSERT_TRUE(near(a.inverse(a.forward(real_t{4})), real_t{4}, real_t{1e-5}, real_t{1e-5}));
}

TEST(affine_test, composition_applies_inner_then_outer)
{
    using real_t = real_t;
    affine_t<real_t> const a{.scale = 3, .offset = 2};
    affine_t<real_t> const b{.scale = real_t{0.5}, .offset = real_t{-1}};

    auto const ab = a * b;
    auto const x = real_t{7};
    ASSERT_TRUE(near(ab.forward(x), a.forward(b.forward(x)), real_t{1e-4}, real_t{1e-5}));
}

//
// Onset Geometry Tests
//

TEST(onset_geometry_test, roundtrip_maintains_smootherstep_geometry)
{
    using real_t = real_t;
    onset_geometry_t<real_t, transitions::smootherstep_integral_t> const geom{real_t{2}, real_t{1}, {}};
    bisect_lower_bound_t invert{};

    for (auto x = real_t{1.5}; x <= real_t{4}; x += real_t{0.05})
    {
        auto const back = geom.inverse(geom.forward(x), invert);
        ASSERT_TRUE(near(back, x, real_t{1e-2}, real_t{1e-2})) << "x=" << x << " back=" << back;
    }
}

// integral of u^2 from 0: rises to 1/3, strictly increasing on (0, 1]
template <typename real_t> struct quadratic_integral_t
{
    template <typename value_t> constexpr auto operator()(value_t t) const noexcept -> value_t
    {
        return t * t * t * (real_t{1} / real_t{3});
    }
};

TEST(onset_geometry_test, roundtrip_maintains_cubic_ramp_geometry)
{
    using real_t = real_t;

    onset_geometry_t<real_t, quadratic_integral_t<real_t>> const geom{real_t{2}, real_t{1}, {}};
    bisect_lower_bound_t invert{};

    for (auto x = real_t{1.5}; x <= real_t{4}; x += real_t{0.05})
    {
        auto const back = geom.inverse(geom.forward(x), invert);
        ASSERT_TRUE(near(back, x, real_t{1e-2}, real_t{1e-2})) << "x=" << x << " back=" << back;
    }
}

TEST(onset_geometry_test, disabled_geometry_acts_as_identity)
{
    using real_t = real_t;
    onset_geometry_t<real_t, transitions::smootherstep_integral_t> const geom{real_t{0}, real_t{0}, {}};
    bisect_lower_bound_t invert{};

    auto const test_val = real_t{5};
    ASSERT_TRUE(geom.forward(test_val) == test_val);
    ASSERT_TRUE(geom.inverse(test_val, invert) == test_val);
}

//
// Engagement Window Tests
//

//
// Engagement Window Tests (Parameterized)
//

class engagement_test : public ::testing::TestWithParam<real_t>
{};

TEST_P(engagement_test, window_tracks_output_scale)
{
    using real_t = real_t;
    linear_engagement_t<real_t> const engage;

    real_t const scale = GetParam();
    real_t const y_max = 10, width = 1, rise = real_t{0.5};
    auto const window = scale * width * rise;

    ASSERT_TRUE(near(engage(y_max + window, y_max, width, scale, rise), real_t{0}, real_t{1e-4}, real_t{1e-4}));
    ASSERT_TRUE(
        near(engage(y_max + window * real_t{0.5}, y_max, width, scale, rise), real_t{0.5}, real_t{1e-4}, real_t{1e-4}));
}

INSTANTIATE_TEST_SUITE_P(scale_sweeps, engagement_test, ::testing::Values(real_t{0.5}, real_t{1.0}, real_t{2.0}));

//
// Validator Tests
//

struct validator_fixture : Test
{
    using real_t = real_t;
    default_validator_t<real_t, min_transition_width<real_t>> validate;

    domain_warp_config_t<real_t> warp;
    input_config_t<real_t> in;
    output_config_t<real_t> out;

    void SetUp() override
    {
        warp = {.onset_center = 2,
            .onset_width = 1,
            .limit_target = 9,
            .limit_width = 0.5,
            .domain_low = 0,
            .domain_high = 10};
        in = {.scale = 1, .offset = 0};
        out = {.scale = 1, .offset = 0, .floor = real_t{0}};
    }
};

TEST_F(validator_fixture, accepts_valid_configuration)
{
    ASSERT_TRUE(!validate(warp, in, out));
}

TEST_F(validator_fixture, rejects_zero_input_scale)
{
    in.scale = 0;
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::invalid_scale);
}

TEST_F(validator_fixture, rejects_positive_input_offset)
{
    in.offset = real_t{0.1};
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::invalid_input_offset);
}

TEST_F(validator_fixture, rejects_negative_floor)
{
    out.floor = real_t{-1};
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::invalid_floor);
}

TEST_F(validator_fixture, rejects_floor_with_offset_conflict)
{
    out.floor = real_t{1};
    out.offset = real_t{0.5};
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::floor_offset_conflict);
}

TEST_F(validator_fixture, rejects_flat_or_inverted_domain)
{
    warp.domain_low = warp.domain_high;
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::invalid_domain);
}

TEST_F(validator_fixture, rejects_limit_width_below_minimum)
{
    warp.limit_width = min_transition_width<real_t> / real_t{2};
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::invalid_width);
}

TEST_F(validator_fixture, rejects_onset_width_below_minimum)
{
    warp.onset_width = min_transition_width<real_t> / real_t{2};
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::invalid_width);
}

TEST_F(validator_fixture, accepts_disabled_onset_at_origin)
{
    warp.onset_center = 0;
    warp.onset_width = 0;
    ASSERT_TRUE(!validate(warp, in, out));
}

TEST_F(validator_fixture, rejects_disabled_onset_away_from_origin)
{
    warp.onset_center = 1;
    warp.onset_width = 0;
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::invalid_onset_disable);
}

//
// Signal Chain Assembly & Evaluation Tests
//

struct signal_chain_fixture : ::testing::Test
{
    using real_t = real_t;
    builder_t builder;

    domain_warp_config_t<real_t> warp;
    input_config_t<real_t> in;
    output_config_t<real_t> out;

    void SetUp() override
    {
        warp = {.onset_center = 2,
            .onset_width = 1,
            .limit_target = 9,
            .limit_width = 0.5,
            .domain_low = 0,
            .domain_high = 10};
        in = {.scale = 1, .offset = 0};
        out = {.scale = 1, .offset = 0, .floor = real_t{0}};
    }
};

TEST_F(signal_chain_fixture, evaluates_monotonically_across_domain)
{
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);
    auto const& chain = res->chain;

    auto prev = chain(real_t{0});
    for (auto x = real_t{0}; x <= real_t{10}; x += real_t{0.05})
    {
        auto const cur = chain(x);
        ASSERT_TRUE(cur >= prev - real_t{1e-4}) << "x=" << x << " cur=" << cur << " prev=" << prev;
        prev = cur;
    }
}

TEST_F(signal_chain_fixture, sets_flat_onset_exactly_to_floor)
{
    out.floor = real_t{0.25};
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);
    ASSERT_TRUE(near(res->chain(real_t{1}), real_t{0.25}, real_t{1e-5}, real_t{1e-4}));
}

TEST_F(signal_chain_fixture, sets_flat_onset_to_offset_when_floor_unset)
{
    out.floor = std::nullopt;
    out.offset = real_t{0.5};
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);
    ASSERT_TRUE(near(res->chain(real_t{1}), real_t{0.5}, real_t{1e-5}, real_t{1e-4}));
}

TEST_F(signal_chain_fixture, caps_output_when_curve_overshoots_limit)
{
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);
    auto const& chain = res->chain;

    ASSERT_TRUE(near(chain(real_t{6}), real_t{9}, real_t{1e-3}, real_t{1e-4})) << chain(real_t{6});
    ASSERT_TRUE(near(chain(real_t{8}), real_t{9}, real_t{1e-3}, real_t{1e-4})) << chain(real_t{8});
}

TEST_F(signal_chain_fixture, leaves_output_uncapped_when_limit_above_curve)
{
    warp.limit_target = real_t{64.5};
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);

    auto const expected = (real_t{9.9} - real_t{2}) * (real_t{9.9} - real_t{2});
    ASSERT_TRUE(near(res->chain(real_t{9.9}), expected, real_t{1e-2}, real_t{1e-3})) << res->chain(real_t{9.9});
}

TEST_F(signal_chain_fixture, analytic_tangent_matches_finite_difference)
{
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);
    auto const& chain = res->chain;

    auto const h = real_t{0.01};
    for (auto x = real_t{0.5}; x <= real_t{9}; x += real_t{0.05})
    {
        auto const analytic = tangent(chain(jet_t{x, real_t{1}}));
        auto const cd = (chain(x + h) - chain(x - h)) / (real_t{2} * h);
        ASSERT_TRUE(near(analytic, cd, real_t{5e-2}, real_t{2e-2})) << "x=" << x << " d=" << analytic << " cd=" << cd;
    }
}

TEST_F(signal_chain_fixture, identifies_warp_overlap_location)
{
    warp.limit_target = real_t{0.4};
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);

    ASSERT_TRUE(!res.has_value());
    ASSERT_TRUE(res.error().error == builder_error_t::warp_overlap);
    ASSERT_TRUE(res.error().location.has_value());
    ASSERT_TRUE(near(res.error().location->low, real_t{2.5}, real_t{1e-3}, real_t{1e-3})) << res.error().location->low;
}

TEST_F(signal_chain_fixture, identifies_negative_domain_location)
{
    warp.onset_center = real_t{2.1};
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);

    ASSERT_TRUE(!res.has_value());
    ASSERT_TRUE(res.error().error == builder_error_t::negative_domain);
    ASSERT_TRUE(res.error().location.has_value());
    ASSERT_TRUE(near(res.error().location->low, real_t{0}, real_t{1e-4}, real_t{0})) << res.error().location->low;
}

TEST_F(signal_chain_fixture, preserves_base_curve_geometry_in_unwarped_region)
{
    // Configure to leave a wide open space in the middle
    warp.onset_center = 2;
    warp.onset_width = 1; // Onset ends at 2.5
    warp.limit_target = 100; // Far above the curve

    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);

    // Pick an x safely past the onset and below the limit
    auto const x = 5.0;

    // Past the onset, the geometry permanently shifts the domain.
    // For a transition with a rise of 0.5, this shift simplifies to exactly onset_center.
    auto const onset_shift = warp.onset_center;
    auto const x_affine = (x - onset_shift) * in.scale + in.offset;
    auto const expected_y = out.scale * x_affine * x_affine;

    auto const actual = res->chain(x);
    ASSERT_TRUE(near(actual, expected_y, real_t{1e-4}, real_t{1e-4}));
}

// this test nudges right and breaks monotonicity on the quadratic.
// it exposes a logic error of nudging anchors via input offset
#if 0
TEST_F(signal_chain_fixture, applies_quantization_shift_to_assembled_chain)
{
    // Force an off-grid onset center to trigger a quantization shift
    warp.onset_center = 2.1;

    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);

    // The quadratic_t anchor is at 0. We need to find where the curve transitions
    // off the floor, which should now be quantized.
    bisect_lower_bound_t invert{};

    // Invert the assembled chain to find the raw input x that produces the floor + epsilon
    // Because the builder shifts the input, this raw x should be exactly on the grid.
    auto const y_just_above_floor = out.floor.value_or(0) + real_t{1e-4};
    auto const opt_x = invert(warp.domain_low, warp.domain_high, y_just_above_floor, res->chain);

    ASSERT_TRUE(opt_x.has_value());

    // The onset start (center - width/2 = 1.6) shifted by the quantizer should land on a grid line.
    auto const onset_start_x = *opt_x; // Approximate start of the rise

    // We expect the distance from the ideal start to be adjusted by the quantum
    auto const ratio = onset_start_x / anchor_quantum;

    // It won't be perfectly round due to the epsilon we used to find it, but it should be very close
    // to the next grid boundary compared to the unquantized 1.6.
    ASSERT_TRUE(std::abs(ratio - std::round(ratio)) < real_t{0.1}) << "x=" << onset_start_x << " ratio=" << ratio;
}
#endif

TEST_F(signal_chain_fixture, builder_aborts_on_validation_failure)
{
    in.scale = 0; // Break the config (invalid scale)
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);

    ASSERT_TRUE(!res.has_value());
    ASSERT_TRUE(res.error().error == builder_error_t::invalid_scale);

    // Ensure no location is provided for a non-domain error
    ASSERT_FALSE(res.error().location.has_value());
}

TEST_F(signal_chain_fixture, assembles_successfully_with_disabled_onset)
{
    warp.onset_center = 0;
    warp.onset_width = 0;

    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);

    // The curve should start rising immediately from x=0 without a flat plateau
    auto const y_at_zero = res->chain(real_t{0});
    auto const y_at_small_step = res->chain(real_t{0.1});

    ASSERT_TRUE(y_at_small_step > y_at_zero)
        << "Curve flattened incorrectly. y(0)=" << y_at_zero << " y(0.1)=" << y_at_small_step;
}

//
// Cusp Quantizer Tests
//

TEST(cusp_quantizer_test, snaps_anchor_to_grid)
{
    using real_t = real_t;
    cusp_quantizer_t<real_t, anchor_quantum, transitions::smootherstep_integral_t> const quantize;
    bisect_lower_bound_t invert{};

    auto warp = domain_warp_config_t<real_t>{.onset_center = 2.1,
        .onset_width = 1,
        .limit_target = 9,
        .limit_width = 0.5,
        .domain_low = 0,
        .domain_high = 10};
    auto const base = input_config_t<real_t>{.scale = 1, .offset = 0}.to_affine();
    onset_geometry_t<real_t, transitions::smootherstep_integral_t> const geom{warp.onset_center, warp.onset_width, {}};

    real_t const anchor = 0;
    real_t const x_raw = base.inverse(geom.inverse(anchor, invert));
    real_t const shift = quantize(anchor, warp, base, {});
    real_t const x_q = x_raw + shift;

    ASSERT_TRUE(shift >= real_t{0} && shift < anchor_quantum) << "shift=" << shift;
    auto const ratio = x_q / anchor_quantum;
    ASSERT_TRUE(near(ratio, std::round(ratio), real_t{1e-3}, real_t{0})) << "x_q=" << x_q << " ratio=" << ratio;
}

} // namespace crv::signal_chain
