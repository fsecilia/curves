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
namespace {

template <typename src_t>
concept has_scalar = requires {
    typename src_t::scalar_t;
    requires std::floating_point<typename src_t::scalar_t>;
};

template <typename evaluator_t>
concept is_anchorable = has_scalar<evaluator_t> && requires(evaluator_t evaluator, evaluator_t::scalar_t scalar) {
    { evaluator.anchor() } -> std::convertible_to<typename evaluator_t::scalar_t>;
    { evaluator.anchor(scalar) };
};

namespace transitions {

// transition based on integral of smootherstep
struct smootherstep_integral_t
{
    // returns normalized integral of smootherstep ranging over (t, y, y') in [(0, 0, 0), (1, 0.5, 1)]
    template <typename value_t> constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        using scalar_t = scalar_type_t<value_t>;

        auto const t = primal(input);
        if (t <= scalar_t{0}) return value_t{0};
        if (t >= scalar_t{1}) return input - scalar_t{0.5}; // input - 1 + f(1)

        // strictly compute using the primal scalar to avoid implicit jet evaluation
        auto const t2 = t * t;
        auto const t4 = t2 * t2;

        // f = t^6 - 3t^5 + 2.5t^4
        auto const y = t4 * (t2 - scalar_t{3} * t + scalar_t{2.5});

        if constexpr (is_jet<value_t>)
        {
            // df = 6t^5 - 15t^4 + 10t^3
            auto const t3 = t2 * t;
            auto const dt = tangent(input);
            auto const dy_dt = t3 * ((scalar_t{6} * t - scalar_t{15}) * t + scalar_t{10});
            return value_t{y, dy_dt * dt};
        }
        else return y;
    }
};

} // namespace transitions

//
// transforms
//

template <std::floating_point real_t> struct affine_t
{
    real_t scale{1};
    real_t shift{0};

    // applies transform forward
    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return x * scale + shift;
    }

    // composes transforms: outer(inner(x))
    [[nodiscard]] constexpr auto operator*(affine_t const& inner) const noexcept -> affine_t
    {
        return affine_t{.scale = scale * inner.scale, .shift = scale * inner.shift + shift};
    }

    // returns inverse transform
    [[nodiscard]] constexpr auto invert() const noexcept -> affine_t
    {
        assert(scale != real_t{0});
        auto const inv_scale = real_t{1} / scale;
        return affine_t{.scale = inv_scale, .shift = -shift * inv_scale};
    }
};

/// geometry of an offset transition
///
/// This type shapes the offset via function composition. It controls the rate at which the composed function advances,
/// starting paused, then smoothly returning to full running speed via the given transition.
///
/// It has an identity state when width == 0.
template <typename t_real_t, typename transition_t> class offset_geometry_t
{
public:
    using real_t = t_real_t;

    constexpr offset_geometry_t(real_t start, real_t width, transition_t transition) noexcept
        : start_{start}, width_{width}, rise_{transition(real_t{1})}, transition_{std::move(transition)}
    {
        assert(width_ >= real_t{0});
        inv_width_ = width_ > real_t{0} ? real_t{1} / width_ : real_t{0};
    }

    // shapes input, pausing at 0, then smoothly returns to full running speed via given transition.
    template <typename value_t> [[nodiscard]] constexpr auto forward(value_t input) const noexcept -> value_t
    {
        if (width_ == real_t{0}) return input;

        auto const x = primal(input);
        if (x <= start_) return value_t{0};
        if (x >= start_ + width_) return input - start_ - width_ * (real_t{1} - rise_);
        return width_ * transition_((input - start_) * inv_width_);
    }

    // inverts forward()
    template <typename invert_t>
    [[nodiscard]] constexpr auto inverse(real_t warped, invert_t const& invert) const -> real_t
    {
        if (width_ == real_t{0}) return warped;
        if (warped <= real_t{0}) return start_;
        if (warped >= width_ * rise_) return warped + start_ + width_ * (real_t{1} - rise_);

        auto const t = invert(real_t{0}, real_t{1}, warped / width_, transition_);
        return start_ + t.value_or(real_t{0}) * width_;
    }

private:
    real_t start_;
    real_t width_;
    real_t inv_width_;
    real_t rise_;
    transition_t transition_;
};

/// geometry of a limit transition
///
/// This type shapes the limit via function composition. It controls the rate at which the composed function advances,
/// starting at full running speed, then smoothly pausing via the given transition.
template <typename t_real_t, typename transition_t> class limit_geometry_t
{
public:
    using real_t = t_real_t;

    constexpr limit_geometry_t(real_t start, real_t width, transition_t transition) noexcept
        : start_{start}, width_{width}, rise_{transition(real_t{1})}, transition_{std::move(transition)}
    {
        assert(width > real_t{0});
        inv_width_ = real_t{1} / width;
    }

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        using scalar_t = scalar_type_t<value_t>;
        auto const x = primal(input);

        if (x <= start_) return input;
        if (x >= start_ + width_) return value_t{start_ + width_ * rise_};

        auto const t = (input - start_) * inv_width_;
        auto const w_int = transition_(scalar_t{1} - t);
        return start_ + width_ * (rise_ - w_int);
    }

private:
    real_t start_;
    real_t width_;
    real_t inv_width_;
    real_t rise_;
    transition_t transition_;
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
        return prev(map(x));
    }
};

template <typename real_t, typename prev_t> struct output_affine_t
{
    affine_t<real_t> map;
    prev_t prev;

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return map(prev(x));
    }
};

template <typename geometry_t, typename prev_t> class offset_warp_t
{
public:
    using real_t = geometry_t::real_t;

    constexpr offset_warp_t(geometry_t geometry, prev_t prev) noexcept
        : geometry_{std::move(geometry)}, prev_{std::move(prev)}
    {}

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        return prev_(geometry_.forward(input));
    }

private:
    geometry_t geometry_;
    prev_t prev_;
};

template <typename geometry_t, typename prev_t> class limit_warp_t
{
public:
    using real_t = geometry_t::real_t;

    constexpr limit_warp_t(geometry_t geometry, real_t blend, prev_t prev) noexcept
        : geometry_{std::move(geometry)}, blend_{blend}, prev_{std::move(prev)}
    {}

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        using scalar_t = scalar_type_t<value_t>;

        if (blend_ <= scalar_t{0}) return prev_(input);

        auto const warped_x = geometry_(input);
        auto const final_x = input + (warped_x - input) * scalar_t{blend_};

        return prev_(final_x);
    }

private:
    geometry_t geometry_;
    real_t blend_;
    prev_t prev_;
};

//
// builder
//

template <typename real_t> struct input_config_t
{
    real_t scale;
    real_t shift;

    [[nodiscard]] constexpr auto to_affine() const noexcept -> affine_t<real_t>
    {
        return affine_t<real_t>{.scale = scale, .shift = -(shift * scale)};
    }
};

template <typename real_t> struct output_config_t
{
    real_t scale;
    real_t shift;
    std::optional<real_t> floor;

    [[nodiscard]] constexpr auto to_affine(real_t y_origin) const noexcept -> affine_t<real_t>
    {
        auto const target_floor = floor.value_or(y_origin + shift);
        return affine_t<real_t>{.scale = scale, .shift = target_floor - y_origin * scale};
    }
};

template <typename real_t> struct domain_warp_config_t
{
    real_t offset_start;
    real_t offset_width;
    real_t offset_height;
    real_t limit_target;
    real_t limit_width;
    real_t limit_height;
    real_t domain_low;
    real_t domain_high;
};

enum class builder_error_t
{
    invalid_width,
    invalid_domain,
    invalid_scale,
    invalid_floor,
    invalid_input_shift,
    negative_domain,
    warp_overlap,
    invalid_offset_disable,
    floor_shift_conflict
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
        case builder_error_t::invalid_input_shift: return "Input must shift left.";
        case builder_error_t::negative_domain: return "Domain mapped to negative curve space, breaking monotonicity.";
        case builder_error_t::warp_overlap: return "Offset and limit transitions overlap.";
        case builder_error_t::invalid_offset_disable:
            return "Disabled offset (zero width) requires offset_center == 0.";
        case builder_error_t::floor_shift_conflict: return "Floor and a nonzero output shift are mutually exclusive.";
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
        // offset_width == 0 disables offset as identity
        if (warp.offset_width < real_t{0}) return builder_error_t::invalid_width;
        if (warp.offset_width == real_t{0})
        {
            // disabled offset must not be positioned
            if (warp.offset_start != real_t{0}) return builder_error_t::invalid_offset_disable;
        }
        else if (warp.offset_width <= min_width) { return builder_error_t::invalid_width; }
        if (warp.limit_width <= min_width) return builder_error_t::invalid_width;

        if (warp.domain_low < real_t{0} || warp.domain_low >= warp.domain_high) return builder_error_t::invalid_domain;
        if (in.scale <= real_t{0} || out.scale <= real_t{0}) return builder_error_t::invalid_scale;
        if (out.floor && *out.floor < real_t{0}) return builder_error_t::invalid_floor;
        if (out.floor && out.shift != real_t{0}) return builder_error_t::floor_shift_conflict;
        if (in.shift > real_t{0}) return builder_error_t::invalid_input_shift;
        return std::nullopt;
    }
};

//
// anchor quantization
//

template <typename real_t, real_t anchor_quantum, typename transition_t, typename invert_t = bisect_lower_bound_t>
struct anchor_quantizer_t
{
    static_assert(anchor_quantum > real_t{0}, "anchor_quantum must be positive (logic error, not user-facing)");

    invert_t invert;

    template <typename curve_t>
    constexpr void operator()(curve_t& curve, domain_warp_config_t<real_t> const& warp,
        affine_t<real_t> const& base_input_map, affine_t<real_t> const& base_input_map_inverse,
        transition_t const& transition) const
    {
        if constexpr (is_anchorable<curve_t>)
        {
            auto const p = static_cast<real_t>(curve.anchor());
            auto const geometry
                = offset_geometry_t<real_t, transition_t>{warp.offset_start, warp.offset_width, transition};

            auto const x_warp_in = geometry.inverse(p, invert);
            auto const x_raw = base_input_map_inverse(x_warp_in);

            if (x_raw <= warp.domain_high)
            {
                auto const x_q = std::ceil(x_raw / anchor_quantum) * anchor_quantum;
                auto const p_prime = geometry.forward(base_input_map(x_q));

                curve.anchor(static_cast<typename curve_t::scalar_t>(p_prime));
            }
        }
    }
};

//
// limit placement
//

template <typename real_t, typename invert_t = bisect_lower_bound_t> struct limit_locator_t
{
    invert_t invert;

    /// \pre offset_chain is monotonic non-decreasing on [input_min, input_max]
    template <typename chain_t>
    [[nodiscard]] constexpr auto operator()(real_t limit, real_t transition_width, real_t transition_height,
        real_t input_min, real_t input_max, chain_t const& offset_chain) const -> real_t
    {
        auto const opt_x_cap = invert(input_min, input_max, limit, offset_chain);
        auto const x_cap = opt_x_cap.value_or(input_max);
        auto const limit_start = x_cap - transition_width * transition_height;

        return limit_start;
    }
};

// rolls on limiter as curve's top approaches limit from below
template <typename real_t> struct linear_engagement_t
{
    /// determines partial engagement of limiter as function of distance from y_max to y_limit over y_engagement_height
    ///
    /// This function returns the linear blend fraction between limited and unlimited curves allowing the limiter to
    /// roll on partially instead of instantly jumping from no engagement to full engagement. Engagement starts at
    /// y_limit - yd_window and increases linearly to y_limit.
    ///
    /// \param y_max maximum signal
    /// \param y_limit configured limit
    /// \param y_height height of engagment window below limit
    /// \returns 0 when function is unengaged with limiter, 1 when fully engaged, linear fraction when partially engaged
    [[nodiscard]] constexpr auto operator()(real_t y_max, real_t y_limit, real_t y_height) const noexcept -> real_t
    {
        if (y_limit <= y_max) return real_t{1};

        auto const yd = y_limit - y_max;
        if (y_height <= yd) return real_t{0};

        return real_t{1} - yd / y_height;
    }
};

//
// builder
//

template <typename real_t, real_t min_transition_width, real_t anchor_quantum,
    typename transition_t = transitions::smootherstep_integral_t,
    typename quantizer_t = anchor_quantizer_t<real_t, anchor_quantum, transition_t>,
    typename locator_t = limit_locator_t<real_t>, typename engagement_t = linear_engagement_t<real_t>,
    typename validator_t = default_validator_t<real_t, min_transition_width>>
class signal_chain_builder_t
{
    transition_t transition_;
    quantizer_t align_anchor_;
    locator_t locate_limit_;
    engagement_t engage_;
    validator_t validate_;

    using offset_geometry_t = offset_geometry_t<real_t, transition_t>;
    using limit_geometry_t = limit_geometry_t<real_t, transition_t>;

    template <typename curve_t> using out_stack_t = output_affine_t<real_t, curve_t>;
    template <typename curve_t> using offset_chain_t = offset_warp_t<offset_geometry_t, out_stack_t<curve_t>>;
    template <typename curve_t> using limit_chain_t = limit_warp_t<limit_geometry_t, offset_chain_t<curve_t>>;
    template <typename curve_t> using final_chain_t = input_affine_t<real_t, limit_chain_t<curve_t>>;
    template <typename curve_t> using result_t = builder_result_t<final_chain_t<curve_t>>;

public:
    constexpr signal_chain_builder_t(transition_t transition = {}, quantizer_t align_anchor = {},
        locator_t locate_limit = {}, engagement_t engage = {}, validator_t validate = {})
        : transition_{std::move(transition)}, align_anchor_{std::move(align_anchor)},
          locate_limit_{std::move(locate_limit)}, engage_{std::move(engage)}, validate_{std::move(validate)}
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
            std::optional<domain_span_t<real_t>> where;
            if (*err == builder_error_t::invalid_domain)
            {
                where = domain_span_t<real_t>{
                    std::min(warp.domain_low, warp.domain_high), std::max(warp.domain_low, warp.domain_high)};
            }
            return std::unexpected(build_error_t<real_t>{*err, where});
        }

        //
        // anchor alignment
        //

        auto const input_map = in.to_affine();
        auto const input_map_inverse = input_map.invert();

        // mutate the local curve copy in place if it models the is_anchorable concept
        align_anchor_(curve, warp, input_map, input_map_inverse, transition_);

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

        auto offset_chain
            = offset_warp_t{offset_geometry_t{warp.offset_start, warp.offset_width, transition_}, std::move(out_stack)};

        auto const input_low = input_map(warp.domain_low);
        if (input_low < real_t{0})
        {
            // breaking slice: [domain_low, where curve space crosses 0], carried back to raw input
            return std::unexpected(build_error_t<real_t>{builder_error_t::negative_domain,
                domain_span_t<real_t>{warp.domain_low, input_map_inverse(real_t{0})}});
        }

        auto const input_high = input_map(warp.domain_high);

        //
        // limit placement
        //

        auto const limit_start = locate_limit_(
            warp.limit_target, warp.limit_width, warp.limit_height, input_low, input_high, offset_chain);

        if (limit_start < warp.offset_start + warp.offset_width)
        {
            auto const offset_end_raw = input_map_inverse(warp.offset_start + warp.offset_width);
            return std::unexpected(build_error_t<real_t>{
                builder_error_t::warp_overlap, domain_span_t<real_t>{offset_end_raw, offset_end_raw}});
        }

        auto const blend = engage_(offset_chain(input_high), warp.limit_target, warp.limit_width * out.scale);
        auto limit_chain = limit_warp_t{
            limit_geometry_t{limit_start, warp.limit_width, transition_}, blend, std::move(offset_chain)};

        //
        // final assembly
        //

        auto final_chain = input_affine_t<real_t, limit_chain_t<curve_t>>{
            .map = input_map,
            .prev = std::move(limit_chain),
        };

        return result_t<curve_t>{std::move(final_chain)};
    }
};

//
// tests
//

template <std::floating_point real_t> inline constexpr auto min_transition_width = static_cast<real_t>(1e-2);
constexpr auto anchor_quantum = static_cast<float_t>(0x1p-10);

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

    using transition_t = transitions::smootherstep_integral_t;
    auto transition = transition_t{};
    auto const transition_height = transition(1.0);

    auto domain_warp_config = domain_warp_config_t<real_t>{
        .offset_start = 0.25,
        .offset_width = 0.25,
        .offset_height = transition_height,
        .limit_target = 17.0,
        .limit_width = 0.5,
        .limit_height = transition_height,
        .domain_low = 0.0,
        .domain_high = 256.0,
    };

    auto const builder = signal_chain_builder_t<real_t, min_transition_width<real_t>, anchor_quantum, transition_t>{};
    auto const build_result
        = builder.build(quadratic_t<real_t>{}, domain_warp_config, input_config_t<real_t>{.scale = 1.0, .shift = -0.1},
            output_config_t<real_t>{.scale = 1.5, .shift = 0.0, .floor = 0.25}); // floor set -> shift must be 0

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
// affine tests
//

TEST(affine_test, inverse_recovers_original_value)
{
    using real_t = real_t;
    affine_t<real_t> const a{.scale = 3, .shift = 2};
    ASSERT_TRUE(near(a.invert()(a(real_t{4})), real_t{4}, real_t{1e-5}, real_t{1e-5}));
}

TEST(affine_test, composition_applies_inner_then_outer)
{
    using real_t = real_t;
    affine_t<real_t> const a{.scale = 3, .shift = 2};
    affine_t<real_t> const b{.scale = real_t{0.5}, .shift = real_t{-1}};

    auto const ab = a * b;
    auto const x = real_t{7};
    ASSERT_TRUE(near(ab(x), a(b(x)), real_t{1e-4}, real_t{1e-5}));
}

//
// offset geometry tests
//

TEST(offset_geometry_test, roundtrip_maintains_smootherstep_geometry)
{
    using real_t = real_t;
    offset_geometry_t<real_t, transitions::smootherstep_integral_t> const geom{real_t{1.5}, real_t{1}, {}};
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

TEST(offset_geometry_test, roundtrip_maintains_cubic_ramp_geometry)
{
    using real_t = real_t;

    offset_geometry_t<real_t, quadratic_integral_t<real_t>> const geom{real_t{1.5}, real_t{1}, {}};
    bisect_lower_bound_t invert{};

    auto const start_x = real_t{2} - real_t{1} * (real_t{1} / real_t{3}); // 5/3
    for (auto x = start_x; x <= real_t{4}; x += real_t{0.05})
    {
        auto const back = geom.inverse(geom.forward(x), invert);
        ASSERT_TRUE(near(back, x, real_t{1e-2}, real_t{1e-2})) << "x=" << x << " back=" << back;
    }
}

TEST(offset_geometry_test, disabled_geometry_acts_as_identity)
{
    using real_t = real_t;
    offset_geometry_t<real_t, transitions::smootherstep_integral_t> const geom{real_t{0}, real_t{0}, {}};
    bisect_lower_bound_t invert{};

    auto const test_val = real_t{5};
    ASSERT_TRUE(geom.forward(test_val) == test_val);
    ASSERT_TRUE(geom.inverse(test_val, invert) == test_val);
}

//
// engagement window tests
//

struct engagement_vector_t
{
    real_t expected;
    real_t y_max;

    friend auto operator<<(std::ostream& out, engagement_vector_t const& src) -> std::ostream&
    {
        return out << "{expected = " << src.expected << ", y_max = " << src.y_max << "}";
    }
};

struct engagement_test_t : public TestWithParam<engagement_vector_t>
{
    static constexpr auto y_limit = 10.0;
    static constexpr auto y_height = 1.0;

    real_t expected = GetParam().expected;
    real_t y_max = GetParam().y_max;

    using sut_t = linear_engagement_t<real_t>;
    sut_t const sut{};
};

TEST_P(engagement_test_t, blend_fraction)
{
    auto const actual = sut(y_max, y_limit, y_height);
    ASSERT_TRUE(near(expected, actual, 1e-4, 1e-4));
}

engagement_vector_t const engagement_vectors[] = {
    // below window, no engagement, clamped to 0% engagement
    {.expected = 0.0, .y_max = 8.5},

    // at bottom edge of window, 0% engagment
    {.expected = 0.0, .y_max = 9.0},

    // halfway through window, 50% engagement
    {.expected = 0.5, .y_max = 9.5},

    // at top edge of window, 100% engagement
    {.expected = 1.0, .y_max = 10.0},

    // above window, clamped to 100% engagement
    {.expected = 1.0, .y_max = 10.5},
};
INSTANTIATE_TEST_SUITE_P(engagement_boundaries, engagement_test_t, ValuesIn(engagement_vectors));

//
// validator tests
//

struct validator_fixture : Test
{
    using real_t = real_t;
    default_validator_t<real_t, min_transition_width<real_t>> validate;

    domain_warp_config_t<real_t> warp = {
        .offset_start = 1,
        .offset_width = 1,
        .offset_height = 0.5,
        .limit_target = 9,
        .limit_width = 0.5,
        .limit_height = 0.5,
        .domain_low = 0,
        .domain_high = 10,
    };
    input_config_t<real_t> in = {.scale = 1, .shift = 0};
    output_config_t<real_t> out = {.scale = 1, .shift = 0, .floor = real_t{0}};
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

TEST_F(validator_fixture, rejects_positive_input_shift)
{
    in.shift = real_t{0.1};
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::invalid_input_shift);
}

TEST_F(validator_fixture, rejects_negative_floor)
{
    out.floor = real_t{-1};
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::invalid_floor);
}

TEST_F(validator_fixture, rejects_floor_with_shift_conflict)
{
    out.floor = real_t{1};
    out.shift = real_t{0.5};
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::floor_shift_conflict);
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

TEST_F(validator_fixture, rejects_offset_width_below_minimum)
{
    warp.offset_width = min_transition_width<real_t> / real_t{2};
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::invalid_width);
}

TEST_F(validator_fixture, accepts_disabled_offset_at_origin)
{
    warp.offset_start = 0;
    warp.offset_width = 0;
    ASSERT_TRUE(!validate(warp, in, out));
}

TEST_F(validator_fixture, rejects_disabled_offset_away_from_origin)
{
    warp.offset_start = 1;
    warp.offset_width = 0;
    ASSERT_TRUE(validate(warp, in, out) == builder_error_t::invalid_offset_disable);
}

//
// signal chain integration test
//

struct signal_chain_fixture_t : Test
{
    using real_t = real_t;
    builder_t builder;

    domain_warp_config_t<real_t> warp = {
        .offset_start = 1.5,
        .offset_width = 1,
        .offset_height = 0.5,
        .limit_target = 9,
        .limit_width = 0.5,
        .limit_height = 0.5,
        .domain_low = 0,
        .domain_high = 10,
    };
    input_config_t<real_t> in = {.scale = 1, .shift = 0};
    output_config_t<real_t> out = {.scale = 1, .shift = 0, .floor = real_t{0}};
};

TEST_F(signal_chain_fixture_t, evaluates_monotonically_across_domain)
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

TEST_F(signal_chain_fixture_t, sets_flat_offset_exactly_to_floor)
{
    out.floor = real_t{0.25};
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);
    ASSERT_TRUE(near(res->chain(real_t{1}), real_t{0.25}, real_t{1e-5}, real_t{1e-4}));
}

TEST_F(signal_chain_fixture_t, sets_flat_offset_to_shift_when_floor_unset)
{
    out.floor = std::nullopt;
    out.shift = real_t{0.5};
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);
    ASSERT_TRUE(near(res->chain(real_t{1}), real_t{0.5}, real_t{1e-5}, real_t{1e-4}));
}

TEST_F(signal_chain_fixture_t, caps_output_when_curve_overshoots_limit)
{
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);
    auto const& chain = res->chain;

    ASSERT_TRUE(near(chain(real_t{6}), real_t{9}, real_t{1e-3}, real_t{1e-4})) << chain(real_t{6});
    ASSERT_TRUE(near(chain(real_t{8}), real_t{9}, real_t{1e-3}, real_t{1e-4})) << chain(real_t{8});
}

TEST_F(signal_chain_fixture_t, leaves_output_uncapped_when_limit_above_curve)
{
    warp.limit_target = real_t{64.5};
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);

    auto actual = res->chain(real_t{9.9});

    auto const total_shift = warp.offset_start + warp.offset_width * (real_t{1} - warp.offset_height);
    auto const shifted_input = real_t{9.9} - total_shift;
    auto const expected = shifted_input * shifted_input;
    ASSERT_TRUE(near(actual, expected, real_t{1e-2}, real_t{1e-3})) << actual;
}

TEST_F(signal_chain_fixture_t, analytic_tangent_matches_finite_difference)
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

TEST_F(signal_chain_fixture_t, identifies_warp_overlap_location)
{
    warp.limit_target = real_t{0.4};
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);

    ASSERT_TRUE(!res.has_value());
    ASSERT_TRUE(res.error().error == builder_error_t::warp_overlap);
    ASSERT_TRUE(res.error().location.has_value());
    ASSERT_TRUE(near(res.error().location->low, real_t{2.5}, real_t{1e-3}, real_t{1e-3})) << res.error().location->low;
}

TEST_F(signal_chain_fixture_t, preserves_base_curve_geometry_in_unwarped_region)
{
    // configure to leave a wide open space in the middle
    warp.offset_start = 1;
    warp.offset_width = 1; // offset ends at 2.5
    warp.limit_target = 100; // far above the curve

    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);

    // pick an x safely past the offset and below the limit
    auto const x = 5.0;

    // past the offset, the geometry permanently shifts the domain
    auto const offset_shift = warp.offset_start + warp.offset_width * (real_t{1} - warp.offset_height);
    auto const x_affine = (x - offset_shift) * in.scale + in.shift;
    auto const expected = out.scale * x_affine * x_affine;

    auto const actual = res->chain(x);
    ASSERT_TRUE(near(actual, expected, real_t{1e-4}, real_t{1e-4}));
}

TEST_F(signal_chain_fixture_t, builder_aborts_on_validation_failure)
{
    in.scale = 0; // break config with invalid scale
    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);

    ASSERT_TRUE(!res.has_value());
    ASSERT_TRUE(res.error().error == builder_error_t::invalid_scale);

    // ensure no location is provided for a non-domain error
    ASSERT_FALSE(res.error().location.has_value());
}

TEST_F(signal_chain_fixture_t, assembles_successfully_with_disabled_offset)
{
    warp.offset_start = 0;
    warp.offset_width = 0;

    auto const res = builder.build(quadratic_t<real_t>{}, warp, in, out);
    ASSERT_TRUE(res.has_value()) << to_string(res.error().error);

    // curve starts rising immediately from x=0 without a flat plateau
    auto const y_at_zero = res->chain(real_t{0});
    auto const y_at_small_step = res->chain(real_t{0.1});

    ASSERT_TRUE(y_at_small_step > y_at_zero)
        << "Curve flattened incorrectly. y(0)=" << y_at_zero << " y(0.1)=" << y_at_small_step;
}

//
// cusp quantizer tests
//

namespace {

// satisfies the is_anchorable concept
template <typename real_t_ = float_t> struct anchorable_t
{
    using scalar_t = real_t_;
    scalar_t anchor_{0};

    constexpr auto anchor() const noexcept -> scalar_t { return anchor_; }
    constexpr void anchor(scalar_t value) noexcept { anchor_ = value; }

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t { return x * x; }
};

} // namespace

TEST(anchor_aligner_test, snaps_curve_anchor_to_input_grid)
{
    auto const quantize = anchor_quantizer_t<real_t, anchor_quantum, transitions::smootherstep_integral_t>{};

    auto warp = domain_warp_config_t<real_t>{
        .offset_start = 1.1,
        .offset_width = 1.0,
        .offset_height = 0.5,
        .limit_target = 9.0,
        .limit_width = 0.5,
        .limit_height = 0.5,
        .domain_low = 0.0,
        .domain_high = 10.0,
    };
    auto const base_input_map = input_config_t<real_t>{.scale = 1.0, .shift = 0.0}.to_affine();
    auto const base_input_map_inverse = base_input_map.invert();

    anchorable_t<real_t> curve;
    curve.anchor(0.0); // default anchor

    quantize(curve, warp, base_input_map, base_input_map_inverse, {});

    // retrieve mutated anchor
    auto const p_prime = curve.anchor();

    // map new anchor backwards; it lands exactly on a quantum boundary in raw input space
    offset_geometry_t<real_t, transitions::smootherstep_integral_t> const geom{
        warp.offset_start, warp.offset_width, {}};
    bisect_lower_bound_t invert{};

    auto const x_warp_in = geom.inverse(p_prime, invert);
    auto const x_raw = base_input_map_inverse(x_warp_in);

    auto const ratio = x_raw / anchor_quantum;

    ASSERT_TRUE(p_prime > real_t{0}) << "Anchor was not shifted.";
    ASSERT_TRUE(near(ratio, std::round(ratio), real_t{1e-3}, real_t{0})) << "x_raw=" << x_raw << " ratio=" << ratio;
}

} // namespace
} // namespace crv::signal_chain
