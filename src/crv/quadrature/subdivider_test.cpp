// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "subdivider.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <numeric>

namespace crv::quadrature::generic {
namespace {

using scalar_t = float_t;
using segment_t = segment_t<scalar_t>;
using refinement_t = refinement_t<scalar_t>;

using stack_t = std::vector<segment_t>;

struct stub_integral_t
{
    using estimate_t = scalar_t;
    auto estimate(scalar_t, scalar_t) const noexcept -> estimate_t;
    auto integrate(scalar_t, scalar_t) const noexcept -> scalar_t;
};

// ====================================================================================================================
// refinement_predicate_t
// ====================================================================================================================

namespace refinement_predicate_test {

using sut_t = refinement_predicate_t<scalar_t>;
constexpr auto sut = sut_t{};

// --------------------------------------------------------------------------------------------------------------------
// baseline
//
// comfortably inside every limit, so refinement continues
// --------------------------------------------------------------------------------------------------------------------

constexpr auto base_limit = 10;
constexpr auto base_area = 1.0;
constexpr auto base_error = 100.0; // huge error demands refinement
constexpr auto base_min_width = sut_t::min_width;
constexpr auto base_noise = base_area * sut_t::relative_noise_margin;

constexpr auto base_segment = segment_t{
    .left = 0.0,
    .right = base_min_width * 10.0, // plenty of width
    .coarse_integral = 0.0,
    .tolerance = base_noise * 2.0, // segment tolerance dominates noise floor
    .depth = 5, // safely below limit
};

// baseline always passes
static_assert(sut(base_segment, base_area, base_error, base_limit));

// --------------------------------------------------------------------------------------------------------------------
// isolated stops
//
// change one baseline condition at a time; each case should stop refinement
// --------------------------------------------------------------------------------------------------------------------

// depth dominates
static_assert(!sut(base_segment, base_area, base_error, 5));
static_assert(sut(base_segment, base_area, base_error, 5).refinement_limited);

// width dominates
constexpr auto narrow_segment = [] {
    auto segment = base_segment;
    segment.right = segment.left + (base_min_width / 2.0);
    return segment;
}();
static_assert(!sut(narrow_segment, base_area, base_error, base_limit));
static_assert(sut(narrow_segment, base_area, base_error, base_limit).refinement_limited);

// error dominates, falling below tolerance, but above noise floor
//
// base_segment.tolerance is base_noise * 2.0. Set error right between them.
constexpr auto low_error = base_noise * 1.5;
static_assert(!sut(base_segment, base_area, low_error, base_limit));
static_assert(!sut(base_segment, base_area, low_error, base_limit).refinement_limited);

// noise floor dominates, becoming the ceiling
constexpr auto low_tolerance_segment = [] {
    auto segment = base_segment;
    segment.tolerance = base_noise / 2.0; // error is now lower than noise floor
    return segment;
}();

// error is higher than tolerance, but lower than noise floor
constexpr auto noise_dominated_error = base_noise * 0.75;
static_assert(!sut(low_tolerance_segment, base_area, noise_dominated_error, base_limit));

// --------------------------------------------------------------------------------------------------------------------
// edge and boundary cases
// --------------------------------------------------------------------------------------------------------------------

// tolerance tie; segment.tolerance == noise_floor
constexpr auto tie_segment = [] {
    auto segment = base_segment;
    segment.tolerance = base_noise;
    return segment;
}();

// error slightly above tie requires refinement
static_assert(sut(tie_segment, base_area, base_noise + 0.1, base_limit));

// error exactly equal to tie must halt refinement
static_assert(!sut(tie_segment, base_area, base_noise, base_limit));

// zero area; noise floor is zero, so segment tolerance wins
constexpr auto zero_area_segment = [] {
    auto segment = base_segment;
    segment.tolerance = 1.0;
    return segment;
}();
static_assert(sut(zero_area_segment, 0.0, 2.0, base_limit));
static_assert(!sut(zero_area_segment, 0.0, 0.5, base_limit));

// negative area still uses positive noise floor through abs(area)
constexpr auto negative_area = -base_area;
static_assert(!sut(base_segment, negative_area, low_error, base_limit));

// inverted segment has negative width and must stop refinement
constexpr auto inverted_segment = [] {
    auto s = base_segment;
    s.left = 100.0;
    s.right = 0.0; // width is now -100.0
    return s;
}();
static_assert(!sut(inverted_segment, base_area, base_error, base_limit));

// negative error is below any valid positive tolerance
static_assert(!sut(base_segment, base_area, -1.0, base_limit));

} // namespace refinement_predicate_test

// ====================================================================================================================
// subdivider_t
// ====================================================================================================================

namespace subdivider_test {

// accumulates final results
struct builder_t
{
    int_t appended_segment_count = 0;
    scalar_t total_integral = 0.0;
    bool was_refinement_limited = false;

    constexpr auto append(scalar_t, scalar_t integral, scalar_t, bool refinement_limited) -> void
    {
        ++appended_segment_count;
        total_integral += integral;
        was_refinement_limited |= refinement_limited;
    }
};

// bisector that just increments depth and divides the integral
struct stub_bisector_t
{
    constexpr auto operator()(stub_integral_t const&, segment_t const& seg) const -> refinement_t
    {
        return refinement_t
        {
            .left = segment_t
            {
                .left = 0.0,
                .right = seg.right/2.0,
                .coarse_integral = seg.coarse_integral / 2.0,
                .tolerance = 0.0,
                .depth = seg.depth + 1,
            },
            .right = segment_t
            {
                .left = seg.right/2.0,
                .right = seg.right,
                .coarse_integral = seg.coarse_integral / 2.0,
                .tolerance = 0.0,
                .depth = seg.depth + 1,
            },
            .refined_integral = seg.coarse_integral,
            .refined_error = 0.0,
        };
    }
};

// predicate that strictly stops at a given depth
struct stub_predicate_t
{
    using scalar_t = float_t;

    int_t depth;

    constexpr auto operator()(segment_t const& seg, scalar_t, scalar_t, int_t) const noexcept -> bool
    {
        return seg.depth < depth;
    }
};

constexpr auto test_immediate_termination() -> bool
{
    auto stack = stack_t{};
    auto builder = builder_t{};
    auto const initial_segment
        = segment_t{.left = 0.0, .right = 0.0, .coarse_integral = 100.0, .tolerance = 0.0, .depth = 0};

    stack.push_back(initial_segment);

    // predicate stops immediately at depth 0
    auto const sut = subdivider_t<stub_predicate_t>{.should_refine = stub_predicate_t{.depth = 0}};

    sut.run(stack, stub_integral_t{}, stub_bisector_t{}, builder, 10);

    // halts immediately, refines once, fails the predicate, and appends
    return builder.appended_segment_count == 1 && builder.total_integral == 100.0;
}
static_assert(test_immediate_termination());

constexpr auto test_shallow_subdivision() -> bool
{
    auto stack = stack_t{};
    auto builder = builder_t{};
    auto const initial_segment
        = segment_t{.left = 0.0, .right = 0.0, .coarse_integral = 100.0, .tolerance = 0.0, .depth = 0};

    stack.push_back(initial_segment);

    // predicate allows exactly one level of refinement
    auto const sut = subdivider_t<stub_predicate_t>{.should_refine = stub_predicate_t{.depth = 1}};

    sut.run(stack, stub_integral_t{}, stub_bisector_t{}, builder, 10);

    // splits root into 2 segments, which then fail the predicate and append
    return builder.appended_segment_count == 2 && builder.total_integral == 100.0;
}
static_assert(test_shallow_subdivision());

constexpr auto test_structural_limit_is_forwarded_to_builder() -> bool
{
    struct always_limited_predicate_t
    {
        using scalar_t = float_t;

        constexpr auto operator()(segment_t const&, scalar_t, scalar_t, int_t) const noexcept -> refinement_decision_t
        {
            return {.refine = false, .refinement_limited = true};
        }
    };

    auto stack = stack_t{};
    auto builder = builder_t{};
    stack.push_back(segment_t{.left = 0.0, .right = 1.0, .coarse_integral = 1.0, .tolerance = 0.0, .depth = 0});

    auto const sut = subdivider_t<always_limited_predicate_t>{};
    sut.run(stack, stub_integral_t{}, stub_bisector_t{}, builder, 10);

    return builder.appended_segment_count == 1 && builder.was_refinement_limited;
}
static_assert(test_structural_limit_is_forwarded_to_builder());

} // namespace subdivider_test

// ====================================================================================================================
// subdivider_t refine call-count contract
// ====================================================================================================================

namespace subdivider_bisector_contract_test {

struct mock_bisector_t
{
    virtual ~mock_bisector_t() = default;

    MOCK_METHOD(refinement_t, call, (segment_t const&), (const));
};

struct bisector_t
{
    mock_bisector_t* mock;

    auto operator()(stub_integral_t const&, segment_t const& seg) const -> refinement_t { return mock->call(seg); }
};

struct builder_t
{
    int_t appended_segment_count = 0;

    auto append(scalar_t, scalar_t, scalar_t, bool) -> void { ++appended_segment_count; }
};

// predicate that strictly stops at a given depth
struct stub_predicate_t
{
    using scalar_t = float_t;

    int_t depth;

    constexpr auto operator()(segment_t const& seg, scalar_t, scalar_t, int_t) const noexcept -> bool
    {
        return seg.depth < depth;
    }
};

// returns a depth+1 balanced refinement; integral and error pass through unchanged
auto make_balanced_refinement(segment_t const& seg) -> refinement_t
{
    auto const mid = std::midpoint(seg.left, seg.right);
    return refinement_t{
        .left = segment_t{
            .left = seg.left,
            .right = mid,
            .coarse_integral = seg.coarse_integral / 2.0,
            .tolerance = 0.0,
            .depth = seg.depth + 1,
        },
        .right = segment_t{
            .left = mid,
            .right = seg.right,
            .coarse_integral = seg.coarse_integral / 2.0,
            .tolerance = 0.0,
            .depth = seg.depth + 1,
        },
        .refined_integral = seg.coarse_integral,
        .refined_error = 0.0,
    };
}

TEST(quadrature_subdivider_bisector_contract_test, refines_once_per_popped_segment_at_depth_two)
{
    // complete tree through depth 2: 1 + 2 + 4 = 7 refine calls, then four leaves append
    auto stack = stack_t{};
    auto builder = builder_t{};
    stack.push_back(segment_t{.left = 0.0, .right = 4.0, .coarse_integral = 100.0, .tolerance = 0.0, .depth = 0});

    StrictMock<mock_bisector_t> mock_bisector;
    EXPECT_CALL(mock_bisector, call(_)).Times(7).WillRepeatedly(Invoke(make_balanced_refinement));

    auto const sut = subdivider_t<stub_predicate_t>{.should_refine = stub_predicate_t{.depth = 2}};
    sut.run(stack, stub_integral_t{}, bisector_t{&mock_bisector}, builder, 10);

    EXPECT_EQ(builder.appended_segment_count, 4);
}

TEST(quadrature_subdivider_bisector_contract_test, refines_once_when_predicate_stops_at_root)
{
    auto stack = stack_t{};
    auto builder = builder_t{};
    stack.push_back(segment_t{.left = 0.0, .right = 4.0, .coarse_integral = 100.0, .tolerance = 0.0, .depth = 0});

    StrictMock<mock_bisector_t> mock_bisector;
    EXPECT_CALL(mock_bisector, call(_)).Times(1).WillOnce(Invoke(make_balanced_refinement));

    auto const sut = subdivider_t<stub_predicate_t>{.should_refine = stub_predicate_t{.depth = 0}};
    sut.run(stack, stub_integral_t{}, bisector_t{&mock_bisector}, builder, 10);

    EXPECT_EQ(builder.appended_segment_count, 1);
}

} // namespace subdivider_bisector_contract_test

} // namespace
} // namespace crv::quadrature::generic
