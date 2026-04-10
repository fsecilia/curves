// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "subdivider.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::quadrature::generic {
namespace {

using real_t      = float_t;
using segment_t   = segment_t<real_t>;
using bisection_t = bisection_t<real_t>;

// ====================================================================================================================
// subdivision_predicate_t
// ====================================================================================================================

namespace subdivision_predicate_test {

using sut_t        = subdivision_predicate_t<real_t>;
constexpr auto sut = sut_t{};

// --------------------------------------------------------------------------------------------------------------------
// baseline
//
// Everything here is well within limits. Predicate must return true, requiring subdivision.
// --------------------------------------------------------------------------------------------------------------------

constexpr auto base_limit     = 10;
constexpr auto base_area      = 1.0;
constexpr auto base_error     = 100.0; // huge error demands subdivision
constexpr auto base_min_width = sut_t::min_width;
constexpr auto base_noise     = base_area * sut_t::relative_noise_margin;

constexpr auto base_segment = segment_t{
    .left      = 0.0,
    .right     = base_min_width * 10.0, // plenty of width
    .integral  = 0.0,
    .tolerance = base_noise * 2.0,      // segment tolerance dominates noise floor
    .depth     = 5,                     // safely below limit
};

// baseline always passes
static_assert(sut(base_segment, base_area, base_error, base_limit));

// --------------------------------------------------------------------------------------------------------------------
// isolated failures
//
// Each case here isolates one failure condition by altering the baseline. Predicate must return false, indicating
// halt subdivision.
// --------------------------------------------------------------------------------------------------------------------

// depth dominates
static_assert(!sut(base_segment, base_area, base_error, 5));

// width dominates
constexpr auto narrow_segment = [] {
    auto segment  = base_segment;
    segment.right = segment.left + (base_min_width / 2.0);
    return segment;
}();
static_assert(!sut(narrow_segment, base_area, base_error, base_limit));

// error dominates, falling below tolerance, but above noise floor
//
// base_segment.tolerance is base_noise * 2.0. Set error right between them.
constexpr auto low_error = base_noise * 1.5;
static_assert(!sut(base_segment, base_area, low_error, base_limit));

// noise floor dominates, becoming the ceiling
constexpr auto low_tolerance_segment = [] {
    auto segment      = base_segment;
    segment.tolerance = base_noise / 2.0; // error is now lower than noise floor
    return segment;
}();

// error is higher than tolerance, but lower than noise floor
constexpr auto noise_dominated_error = base_noise * 0.75;
static_assert(!sut(low_tolerance_segment, base_area, noise_dominated_error, base_limit));

// --------------------------------------------------------------------------------------------------------------------
// edge and boundary cases
// --------------------------------------------------------------------------------------------------------------------

// tolerance tie, segment.tolerance == noise_floor
//
// logic should still hold when both tolerances are the same
constexpr auto tie_segment = [] {
    auto segment      = base_segment;
    segment.tolerance = base_noise;
    return segment;
}();

// error slightly above tie requires subdivision
static_assert(sut(tie_segment, base_area, base_noise + 0.1, base_limit));

// error exactly equal to tie must halt subdivision
static_assert(!sut(tie_segment, base_area, base_noise, base_limit));

// zero area
//
// noise_floor becomes 0.0, so local_tolerance must fall back to segment.tolerance.
constexpr auto zero_area_segment = [] {
    auto segment      = base_segment;
    segment.tolerance = 1.0;
    return segment;
}();
static_assert(sut(zero_area_segment, 0.0, 2.0, base_limit));
static_assert(!sut(zero_area_segment, 0.0, 0.5, base_limit));

// negative area
//
// std::abs(area) must trigger and prevent a negative noise floor from breaking the math.
// We use low_error to test logic behaves exactly as in positive area test.
constexpr double negative_area = -base_area;
static_assert(!sut(base_segment, negative_area, low_error, base_limit));

// negative width, inverted segment, left > right
//
// If the geometry is corrupted or inverted, current_width becomes negative, which is definitely smaller than min width.
constexpr auto inverted_segment = [] {
    auto s  = base_segment;
    s.left  = 100.0;
    s.right = 0.0; // width is now -100.0
    return s;
}();
static_assert(!sut(inverted_segment, base_area, base_error, base_limit));

// negative error
//
// If error metric drops below zero, it is definitively lower than any valid positive tolerance.
static_assert(!sut(base_segment, base_area, -1.0, base_limit));

} // namespace subdivision_predicate_test

// ====================================================================================================================
// subdivider_t
// ====================================================================================================================

namespace subdivider_test {

using stack_t = std::vector<segment_t>;

// accumulates final results
struct builder_t
{
    int_t  appended_segment_count = 0;
    real_t total_integral         = 0.0;

    constexpr auto append(real_t, real_t integral, real_t) -> void
    {
        ++appended_segment_count;
        total_integral += integral;
    }
};

// bisector that just increments depth and divides the integral
struct stub_bisector_t
{
    constexpr auto operator()(segment_t const& seg) const -> bisection_t
    {
        return bisection_t
        {
            .left           = segment_t
            {
                .left       = 0.0,
                .right      = seg.right/2.0,
                .integral   = seg.integral / 2.0,
                .tolerance  = 0.0,
                .depth      = seg.depth + 1,
            },
            .right          = segment_t
            {
                .left       = seg.right/2.0,
                .right      = seg.right,
                .integral   = seg.integral / 2.0,
                .tolerance  = 0.0,
                .depth      = seg.depth + 1,
            },
            .integral       = seg.integral,
            .error_estimate = 0.0,
        };
    }
};

// predicate that strictly stops at a given depth
struct stub_predicate_t
{
    int_t depth;

    constexpr auto operator()(segment_t const& seg, real_t, real_t, int_t) const noexcept -> bool
    {
        return seg.depth < depth;
    }
};

constexpr auto test_immediate_termination() -> bool
{
    auto stack           = stack_t{};
    auto builder         = builder_t{};
    auto initial_segment = segment_t{.left = 0.0, .right = 0.0, .integral = 100.0, .tolerance = 0.0, .depth = 0};

    stack.push_back(initial_segment);

    // predicate stops immediately at depth 0
    auto sut = subdivider_t<real_t, stub_predicate_t>{.should_subdivide = stub_predicate_t{.depth = 0}};

    sut.run(stack, stub_bisector_t{}, builder, 10);

    // halts immediately, bisects once, fails the predicate, and appends
    return builder.appended_segment_count == 1 && builder.total_integral == 100.0;
}
static_assert(test_immediate_termination());

constexpr auto test_shallow_subdivision() -> bool
{
    auto stack           = stack_t{};
    auto builder         = builder_t{};
    auto initial_segment = segment_t{.left = 0.0, .right = 0.0, .integral = 100.0, .tolerance = 0.0, .depth = 0};

    stack.push_back(initial_segment);

    // predicate allows exactly one level of subdivision
    auto sut = subdivider_t<real_t, stub_predicate_t>{.should_subdivide = stub_predicate_t{.depth = 1}};

    sut.run(stack, stub_bisector_t{}, builder, 10);

    // splits root into 2 segments, which then fail the predicate and append
    return builder.appended_segment_count == 2 && builder.total_integral == 100.0;
}
static_assert(test_shallow_subdivision());

} // namespace subdivider_test

} // namespace
} // namespace crv::quadrature::generic
