// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "interval.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using real_t = float_t;
using jet_t = jet_t<real_t>;

// ====================================================================================================================
// interval_priority_less_t
// ====================================================================================================================

namespace interval_priority_less_tests {

struct segment_t
{};
using sut_t = interval_t<real_t, segment_t>;

constexpr auto defects = segment_defects_t{1};
constexpr auto no_defects = segment_defects_t{0};

constexpr auto construct_sut(segment_defects_t segment_defects, real_t weighted_error, real_t left_x) noexcept -> sut_t
{
    auto result = sut_t{};
    result.subdomain.left.x = left_x;
    result.residual.weighted_error = weighted_error;
    result.segment_defects = segment_defects;
    return result;
}

constexpr auto sut = interval_priority_less_t{};

// --------------------------------------------------------------------------------------------------------------------
// lexicographic dominance
// --------------------------------------------------------------------------------------------------------------------

// defects dominate weighted error
constexpr auto clean_but_huge_error = construct_sut(no_defects, 1e30, 1e30);
constexpr auto defects_but_zero_error = construct_sut(defects, 0.0, 0.0);
static_assert(sut(clean_but_huge_error, defects_but_zero_error));
static_assert(!sut(defects_but_zero_error, clean_but_huge_error));

// weighted error dominates left.x
static_assert(sut(construct_sut(defects, 0.0, 1e30), construct_sut(defects, 1.0, 0.0)));
static_assert(sut(construct_sut(no_defects, 0.0, 1e30), construct_sut(no_defects, 1.0, 0.0)));

// left.x breaks ties
static_assert(sut(construct_sut(defects, 5.0, 1.0), construct_sut(defects, 5.0, 2.0)));
static_assert(!sut(construct_sut(defects, 5.0, 2.0), construct_sut(defects, 5.0, 1.0)));

// --------------------------------------------------------------------------------------------------------------------
// defect equivalence classes
// --------------------------------------------------------------------------------------------------------------------

// strict weak ordering
constexpr auto defect_less = construct_sut(segment_defects_t{1}, 1.0, 1.0);
constexpr auto defect_greater = construct_sut(segment_defects_t{2}, 1.0, 1.0);
static_assert(!sut(defect_less, defect_greater));
static_assert(!sut(defect_greater, defect_less));

// --------------------------------------------------------------------------------------------------------------------
// nonfinite death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

namespace death_tests {

constexpr auto nan = std::numeric_limits<real_t>::quiet_NaN();
constexpr auto inf = std::numeric_limits<real_t>::infinity();

TEST(spline_interval_priority_less, death_by_lhs_residual_weighted_error)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(no_defects, nan, 0.0), construct_sut(no_defects, 0.0, 0.0)), "isfinite");
}

TEST(spline_interval_priority_less, death_by_lhs_left_x)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(no_defects, 0.0, inf), construct_sut(no_defects, 0.0, 0.0)), "isfinite");
}

TEST(spline_interval_priority_less, death_by_rhs_residual_weighted_error)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(no_defects, 0.0, 0.0), construct_sut(no_defects, -inf, 0.0)), "isfinite");
}

TEST(spline_interval_priority_less, death_by_rhs_left_x)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(no_defects, 0.0, 0.0), construct_sut(no_defects, 0.0, nan)), "isfinite");
}

} // namespace death_tests

#endif // #if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace interval_priority_less_tests

// ====================================================================================================================
// interval_factory_t
// ====================================================================================================================

namespace interval_factory_tests {

struct spline_interval_factory_test_t : Test
{
    using x_t = fixed_t<int_t, 0>;

    using subdomain_t = subdomain_t<real_t>;
    using function_sample_t = function_sample_t<real_t>;

    struct segment_t
    {
        using coeffs_t = int_t;
        coeffs_t coeffs_ = 0;
        constexpr auto coeffs() const noexcept -> coeffs_t { return coeffs_; }
        constexpr auto operator==(segment_t const&) const noexcept -> bool = default;
    };

    struct approximant_t
    {
        using x_t = spline_interval_factory_test_t::x_t;

        x_t x0;
        segment_t segment;

        constexpr auto operator==(approximant_t const&) const noexcept -> bool = default;
    };

    struct mock_segment_factory_t
    {
        virtual ~mock_segment_factory_t() = default;
        MOCK_METHOD(segment_t, call, (jet_t left, jet_t right, int_t log2_width), (const, noexcept));
    };
    StrictMock<mock_segment_factory_t> mock_segment_factory;

    struct segment_factory_t
    {
        mock_segment_factory_t* mock = nullptr;
        auto operator()(jet_t left_y, jet_t right_y, int_t log2_width) const noexcept -> segment_t
        {
            return mock->call(left_y, right_y, log2_width);
        }
    };

    struct segment_defects_t
    {
        segment_t segment;
        auto operator==(segment_defects_t const&) const noexcept -> bool = default;
    };

    struct mock_defect_analyzer_t
    {
        virtual ~mock_defect_analyzer_t() = default;
        MOCK_METHOD(segment_defects_t, call, (segment_t::coeffs_t coeffs), (const, noexcept));
    };

    struct defect_analyzer_t
    {
        mock_defect_analyzer_t* mock = nullptr;
        auto operator()(segment_t::coeffs_t coeffs) const noexcept -> segment_defects_t { return mock->call(coeffs); }
    };
    StrictMock<mock_defect_analyzer_t> mock_defect_analyzer;

    struct residual_t
    {
        int_t id = 0;
        auto operator==(residual_t const&) const noexcept -> bool = default;
    };

    struct sample_target_function_t
    {
        int_t id = 0;
        constexpr auto operator==(sample_target_function_t const&) const noexcept -> bool = default;
    };

    struct mock_residual_estimator_t
    {
        virtual ~mock_residual_estimator_t() = default;
        MOCK_METHOD(residual_t, call,
            (sample_target_function_t sample_target_function, approximant_t approximant, real_t left_x, real_t right_x),
            (const, noexcept));
    };
    StrictMock<mock_residual_estimator_t> mock_residual_estimator;

    struct residual_estimator_t
    {
        mock_residual_estimator_t* mock = nullptr;
        auto operator()(sample_target_function_t sample_target_function, approximant_t approximant, real_t left_x,
            real_t right_x) const noexcept -> residual_t
        {
            return mock->call(sample_target_function, approximant, left_x, right_x);
        }
    };

    struct interval_t
    {
        using real_t = real_t;

        subdomain_t subdomain;
        segment_t segment;
        segment_defects_t segment_defects;
        residual_t residual;

        constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
    };

    using sut_t
        = interval_factory_t<interval_t, approximant_t, segment_factory_t, defect_analyzer_t, residual_estimator_t>;
    sut_t sut{.create_segment = segment_factory_t{&mock_segment_factory},
        .analyze_defects = defect_analyzer_t{&mock_defect_analyzer},
        .estimate_residual = residual_estimator_t{&mock_residual_estimator}};

    sample_target_function_t const sample_target_function{1};
    function_sample_t const left{.x = 2.0, .y = {3.0, 4.0}};
    function_sample_t const midpoint{.x = 5.0, .y = {6.0, 7.0}};
    function_sample_t const right{.x = 8.0, .y = {9.0, 10.0}};
    int_t log2_width = 11;
    segment_defects_t const segment_defects{12};
    segment_t const segment{.coeffs_ = 13};
    residual_t const residual{14};

    x_t x0 = to_fixed<x_t>(left.x);

    interval_t const expected
    {
        .subdomain = subdomain_t{
            .left = left,
            .midpoint = midpoint,
            .right = right,
            .log2_width = log2_width,
        },
        .segment = segment,
        .segment_defects = segment_defects,
        .residual = residual,
    };
};

TEST_F(spline_interval_factory_test_t, create)
{
    EXPECT_CALL(mock_segment_factory, call(left.y, right.y, log2_width)).WillOnce(Return(segment));
    EXPECT_CALL(mock_defect_analyzer, call(segment.coeffs())).WillOnce(Return(segment_defects));
    EXPECT_CALL(mock_residual_estimator,
        call(sample_target_function, approximant_t{.x0 = x0, .segment = segment}, left.x, right.x))
        .WillOnce(Return(residual));

    auto const actual = sut.create(sample_target_function, subdomain_t{left, midpoint, right, log2_width});

    EXPECT_EQ(expected, actual);
}

} // namespace interval_factory_tests

} // namespace
} // namespace crv::spline
