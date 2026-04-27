// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stack.hpp"
#include <crv/math/quadrature/segment.hpp>
#include <crv/ranges.hpp>
#include <crv/test/test.hpp>
#include <initializer_list>

namespace crv::quadrature {
namespace {

using real_t = float_t;
using segment_t = segment_t<real_t>;
using refinement_t = refinement_t<real_t>;
using stack_t = std::vector<segment_t>;

// ====================================================================================================================
// stack_seeder_t
// ====================================================================================================================

// These tests rely on segment_t::operator ==(), which tests doubles directly. Currently, they get lucky and the values
// match, but changing any arithmetic in stack_seeder_t, even just the order of operations, will likely break these. If
// they become fragile, the solution is to switch to using a gmock matcher and testing field by field using DoubleNear.

struct quadrature_stack_seeder_test_t : Test
{
    stack_t stack{};

    static auto create_segment(real_t left, real_t right, real_t tolerance, int_t id = 0) noexcept -> segment_t
    {
        return segment_t{
            .left = left,
            .right = right,
            .coarse_integral = left + right + tolerance,
            .tolerance = tolerance,
            .depth = id,
        };
    }

    struct refiner_t
    {
        mutable int_t next_id = 0;

        auto evaluate(real_t left, real_t right, real_t tolerance) const noexcept -> segment_t
        {
            return create_segment(left, right, tolerance, next_id++);
        }
    };
    refiner_t refiner;

    static constexpr auto domain_max = 1024.0;
    static constexpr auto global_tolerance = 1.0;

    using sut_t = stack_seeder_t<real_t>;
    sut_t sut{};
};

// --------------------------------------------------------------------------------------------------------------------
// no critical points
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_seeder_test_no_critical_points_t : quadrature_stack_seeder_test_t
{
    quadrature_stack_seeder_test_no_critical_points_t()
    {
        sut.seed(stack, refiner, domain_max, global_tolerance, std::initializer_list<real_t>{});
    }
};

TEST_F(quadrature_stack_seeder_test_no_critical_points_t, segment)
{
    EXPECT_EQ(stack.back(), create_segment(0.0, domain_max, global_tolerance, 0));
}

// --------------------------------------------------------------------------------------------------------------------
// one critical point
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_seeder_test_one_critical_point_t : quadrature_stack_seeder_test_t
{
    quadrature_stack_seeder_test_one_critical_point_t()
    {
        sut.seed(stack, refiner, domain_max, global_tolerance, std::initializer_list<real_t>{domain_max / 3.0});
    }
};

TEST_F(quadrature_stack_seeder_test_one_critical_point_t, segments)
{
    EXPECT_EQ(stack.back(), create_segment(0.0, domain_max / 3.0, global_tolerance / 3.0, 1));
    stack.pop_back();
    EXPECT_EQ(stack.back(),
        create_segment(
            domain_max / 3.0, domain_max, global_tolerance * (domain_max - domain_max / 3.0) / domain_max, 0));
}

// --------------------------------------------------------------------------------------------------------------------
// many critical points
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_seeder_test_many_critical_points_t : quadrature_stack_seeder_test_t
{
    quadrature_stack_seeder_test_many_critical_points_t()
    {
        sut.seed(stack, refiner, domain_max, global_tolerance,
            std::initializer_list{domain_max / 5.0, domain_max / 3.0, domain_max / 2.0});
    }
};

TEST_F(quadrature_stack_seeder_test_many_critical_points_t, segments)
{
    EXPECT_EQ(stack.back(), create_segment(0.0, domain_max / 5.0, global_tolerance / 5.0, 3));
    stack.pop_back();

    EXPECT_EQ(stack.back(),
        create_segment(domain_max / 5.0, domain_max / 3.0,
            global_tolerance * (domain_max / 3.0 - domain_max / 5.0) / domain_max, 2));
    stack.pop_back();

    EXPECT_EQ(stack.back(),
        create_segment(domain_max / 3.0, domain_max / 2.0,
            global_tolerance * (domain_max / 2.0 - domain_max / 3.0) / domain_max, 1));
    stack.pop_back();

    EXPECT_EQ(stack.back(), create_segment(domain_max / 2.0, domain_max, global_tolerance / 2.0, 0));
}

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct quadrature_stack_seeder_death_tests_t : quadrature_stack_seeder_test_t
{};

TEST_F(quadrature_stack_seeder_death_tests_t, asserts_on_non_empty_stack)
{
    stack.push_back(create_segment(0.0, 1.0, 0.1));

    EXPECT_DEBUG_DEATH(
        sut.seed(stack, refiner, domain_max, global_tolerance, std::initializer_list{0.0}), "empty before seeding");
}

TEST_F(quadrature_stack_seeder_death_tests_t, asserts_on_zero_critical_point)
{
    EXPECT_DEBUG_DEATH(
        sut.seed(stack, refiner, domain_max, global_tolerance, std::initializer_list{0.0}), "in \\(0, domain_max\\)");
}

TEST_F(quadrature_stack_seeder_death_tests_t, asserts_on_negative_critical_point)
{
    EXPECT_DEBUG_DEATH(
        sut.seed(stack, refiner, domain_max, global_tolerance, std::initializer_list{-1.0}), "in \\(0, domain_max\\)");
}

TEST_F(quadrature_stack_seeder_death_tests_t, asserts_on_max_critical_point)
{
    EXPECT_DEBUG_DEATH(sut.seed(stack, refiner, domain_max, global_tolerance, std::initializer_list{domain_max}),
        "in \\(0, domain_max\\)");
}

TEST_F(quadrature_stack_seeder_death_tests_t, asserts_on_unsorted_critical_points)
{
    // passing these in reverse order, descending, should trip the assert
    EXPECT_DEBUG_DEATH(sut.seed(stack, refiner, domain_max, global_tolerance,
                           std::initializer_list{domain_max / 2.0, domain_max / 3.0}),
        "increasing and unique");
}

TEST_F(quadrature_stack_seeder_death_tests_t, asserts_on_duplicate_critical_points)
{
    EXPECT_DEBUG_DEATH(sut.seed(stack, refiner, domain_max, global_tolerance,
                           std::initializer_list{domain_max / 3.0, domain_max / 3.0}),
        "increasing and unique");
}

#endif

} // namespace
} // namespace crv::quadrature
