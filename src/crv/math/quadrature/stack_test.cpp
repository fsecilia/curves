// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stack.hpp"
#include <crv/ranges.hpp>
#include <crv/test/test.hpp>
#include <initializer_list>

namespace crv::quadrature {
namespace {

using real_t      = float_t;
using segment_t   = segment_t<real_t>;
using bisection_t = bisection_t<real_t>;

// ====================================================================================================================
// stack_t
// ====================================================================================================================

struct quadrature_stack_test_t : Test
{
    // uses depth as a simple integer id
    auto create_segment(int_t id) const noexcept -> segment_t
    {
        return segment_t{
            .left      = real_t{0.0},
            .right     = real_t{0.0},
            .integral  = real_t{0.0},
            .tolerance = real_t{0.0},
            .depth     = id,
        };
    }

    auto create_bisection(int_t left_id, int_t right_id) const noexcept -> bisection_t
    {
        return bisection_t{
            .left_child        = create_segment(left_id),
            .right_child       = create_segment(right_id),
            .combined_integral = real_t{0.0},
            .error_estimate    = real_t{0.0},
        };
    }

    using sut_t = stack_t<real_t>;
    sut_t sut{};

    auto expect_order(std::initializer_list<int_t> ids) -> void
    {
        for (auto id : ids | std::views::reverse) { EXPECT_EQ(id, sut.pop().depth); }
    }
};

// --------------------------------------------------------------------------------------------------------------------
// empty stack
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_test_empty_t : quadrature_stack_test_t
{};

TEST_F(quadrature_stack_test_empty_t, starts_empty)
{
    EXPECT_TRUE(sut.empty());
}

TEST_F(quadrature_stack_test_empty_t, clear_succeeds)
{
    sut.clear();
}

TEST_F(quadrature_stack_test_empty_t, pop_on_empty_asserts)
{
    EXPECT_DEBUG_DEATH(sut.pop(), "empty");
}

// --------------------------------------------------------------------------------------------------------------------
// single segment
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_test_single_segment_t : quadrature_stack_test_t
{
    segment_t segment = create_segment(13);

    quadrature_stack_test_single_segment_t() { sut.push(segment); }
};

TEST_F(quadrature_stack_test_single_segment_t, starts_not_empty)
{
    EXPECT_FALSE(sut.empty());
}

TEST_F(quadrature_stack_test_single_segment_t, clears_to_empty)
{
    sut.clear();
    EXPECT_TRUE(sut.empty());
}

TEST_F(quadrature_stack_test_single_segment_t, segments)
{
    expect_order({13});
}

TEST_F(quadrature_stack_test_single_segment_t, pops_to_empty)
{
    sut.pop();
    EXPECT_TRUE(sut.empty());
}

// --------------------------------------------------------------------------------------------------------------------
// single bisection
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_test_single_bisection_t : quadrature_stack_test_t
{
    bisection_t bisection = create_bisection(13, 17);

    quadrature_stack_test_single_bisection_t() { sut.push(bisection); }
};

TEST_F(quadrature_stack_test_single_bisection_t, starts_not_empty)
{
    EXPECT_FALSE(sut.empty());
}

TEST_F(quadrature_stack_test_single_bisection_t, clears_to_empty)
{
    sut.clear();
    EXPECT_TRUE(sut.empty());
}

TEST_F(quadrature_stack_test_single_bisection_t, segments)
{
    expect_order({17, 13});
}

TEST_F(quadrature_stack_test_single_bisection_t, pops_to_empty)
{
    sut.pop();
    sut.pop();
    EXPECT_TRUE(sut.empty());
}

// --------------------------------------------------------------------------------------------------------------------
// multiple_pushed
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_test_multiple_pushed_t : quadrature_stack_test_t
{
    segment_t   segment   = create_segment(13);
    bisection_t bisection = create_bisection(17, 19);

    quadrature_stack_test_multiple_pushed_t()
    {
        sut.push(segment);
        sut.push(bisection);
    }
};

TEST_F(quadrature_stack_test_multiple_pushed_t, starts_not_empty)
{
    EXPECT_FALSE(sut.empty());
}

TEST_F(quadrature_stack_test_multiple_pushed_t, clears_to_empty)
{
    sut.clear();
    EXPECT_TRUE(sut.empty());
}

TEST_F(quadrature_stack_test_multiple_pushed_t, segments)
{
    expect_order({13, 19, 17});
}

TEST_F(quadrature_stack_test_multiple_pushed_t, pops_to_empty)
{
    sut.pop();
    sut.pop();
    sut.pop();
    EXPECT_TRUE(sut.empty());
}

TEST_F(quadrature_stack_test_multiple_pushed_t, interleaved_push_and_pop)
{
    // pop the initial 17
    auto const seg = sut.pop();
    EXPECT_EQ(17, seg.depth);

    // simulate bisection of 13 into 14 and 15
    sut.push(create_bisection(14, 15));

    // pop the left child (14)
    auto const left = sut.pop();
    EXPECT_EQ(14, left.depth);

    // simulate bisection of 15 into 16 and 17
    sut.push(create_bisection(16, 17));

    // remaining order should be the original 13 and 19, then 17 (left of 15), 16 (right of 15), 14 (right of 13)
    expect_order({13, 19, 15, 17, 16});
}

// ====================================================================================================================
// stack_seeder_t
// ====================================================================================================================

// These tests rely on segment_t::operator ==(), which tests doubles directly. Currently, they get lucky and the values
// match, but changing any arithmetic in stack_seeder_t, even just the order of operations, will likely break these
// If they become fragile, the solution is to switch to using a gmock matcher and testing field by field using near.

struct quadrature_stack_seeder_test_t : Test
{
    using stack_t = stack_t<real_t>;
    stack_t stack{};

    static auto create_segment(real_t left, real_t right, real_t tolerance, int_t id = 0) noexcept -> segment_t
    {
        return segment_t{
            .left      = left,
            .right     = right,
            .integral  = left + right + tolerance,
            .tolerance = tolerance,
            .depth     = id,
        };
    }

    struct subdivider_t
    {
        mutable int_t next_id = 0;

        auto operator()(real_t left, real_t right, real_t tolerance) const noexcept -> segment_t
        {
            return create_segment(left, right, tolerance, next_id++);
        }
    };
    subdivider_t subdivider;

    static constexpr auto domain_max       = 1024.0;
    static constexpr auto global_tolerance = 1.0;

    using sut_t = stack_seeder_t<real_t>;
    sut_t sut{};
};

// --------------------------------------------------------------------------------------------------------------------
// single segment
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_seeder_test_single_segment_t : quadrature_stack_seeder_test_t
{
    quadrature_stack_seeder_test_single_segment_t() { sut.seed(stack, subdivider, domain_max, global_tolerance); }
};

TEST_F(quadrature_stack_seeder_test_single_segment_t, segment)
{
    EXPECT_EQ(stack.pop(), create_segment(0.0, domain_max, global_tolerance, 0));
}

// --------------------------------------------------------------------------------------------------------------------
// no critical points
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_seeder_test_no_critical_points_t : quadrature_stack_seeder_test_t
{
    quadrature_stack_seeder_test_no_critical_points_t()
    {
        sut.seed(stack, subdivider, domain_max, global_tolerance, std::initializer_list<real_t>{});
    }
};

TEST_F(quadrature_stack_seeder_test_no_critical_points_t, segment)
{
    EXPECT_EQ(stack.pop(), create_segment(0.0, domain_max, global_tolerance, 0));
}

// --------------------------------------------------------------------------------------------------------------------
// one critical point
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_seeder_test_one_critical_point_t : quadrature_stack_seeder_test_t
{
    quadrature_stack_seeder_test_one_critical_point_t()
    {
        sut.seed(stack, subdivider, domain_max, global_tolerance, std::initializer_list<real_t>{domain_max / 3.0});
    }
};

TEST_F(quadrature_stack_seeder_test_one_critical_point_t, segments)
{
    EXPECT_EQ(stack.pop(), create_segment(0.0, domain_max / 3.0, global_tolerance / 3.0, 1));
    EXPECT_EQ(stack.pop(), create_segment(domain_max / 3.0, domain_max,
                                          global_tolerance * (domain_max - domain_max / 3.0) / domain_max, 0));
}

// --------------------------------------------------------------------------------------------------------------------
// many critical points
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_seeder_test_many_critical_points_t : quadrature_stack_seeder_test_t
{
    quadrature_stack_seeder_test_many_critical_points_t()
    {
        sut.seed(stack, subdivider, domain_max, global_tolerance,
                 std::initializer_list{domain_max / 5.0, domain_max / 3.0, domain_max / 2.0});
    }
};

TEST_F(quadrature_stack_seeder_test_many_critical_points_t, segments)
{
    EXPECT_EQ(stack.pop(), create_segment(0.0, domain_max / 5.0, global_tolerance / 5.0, 3));
    EXPECT_EQ(stack.pop(), create_segment(domain_max / 5.0, domain_max / 3.0,
                                          global_tolerance * (domain_max / 3.0 - domain_max / 5.0) / domain_max, 2));
    EXPECT_EQ(stack.pop(), create_segment(domain_max / 3.0, domain_max / 2.0,
                                          global_tolerance * (domain_max / 2.0 - domain_max / 3.0) / domain_max, 1));
    EXPECT_EQ(stack.pop(), create_segment(domain_max / 2.0, domain_max, global_tolerance / 2.0, 0));
}

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_stack_seeder_death_tests_t : quadrature_stack_seeder_test_t
{};

TEST_F(quadrature_stack_seeder_death_tests_t, single_segment_asserts_on_non_empty_stack)
{
    stack.push(create_segment(0.0, 1.0, 0.1));

    EXPECT_DEBUG_DEATH(sut.seed(stack, subdivider, domain_max, global_tolerance), "empty before seeding");
}

TEST_F(quadrature_stack_seeder_death_tests_t, critical_points_asserts_on_non_empty_stack)
{
    stack.push(create_segment(0.0, 1.0, 0.1));

    EXPECT_DEBUG_DEATH(sut.seed(stack, subdivider, domain_max, global_tolerance, std::initializer_list{0.0}),
                       "empty before seeding");
}

TEST_F(quadrature_stack_seeder_death_tests_t, asserts_on_zero_critical_point)
{
    EXPECT_DEBUG_DEATH(sut.seed(stack, subdivider, domain_max, global_tolerance, std::initializer_list{0.0}),
                       "in \\(0, domain_max\\)");
}

TEST_F(quadrature_stack_seeder_death_tests_t, asserts_on_negative_critical_point)
{
    EXPECT_DEBUG_DEATH(sut.seed(stack, subdivider, domain_max, global_tolerance, std::initializer_list{-1.0}),
                       "in \\(0, domain_max\\)");
}

TEST_F(quadrature_stack_seeder_death_tests_t, asserts_on_max_critical_point)
{
    EXPECT_DEBUG_DEATH(sut.seed(stack, subdivider, domain_max, global_tolerance, std::initializer_list{domain_max}),
                       "in \\(0, domain_max\\)");
}

TEST_F(quadrature_stack_seeder_death_tests_t, asserts_on_unsorted_critical_points)
{
    // passing these in reverse order, descending, should trip the assert
    EXPECT_DEBUG_DEATH(sut.seed(stack, subdivider, domain_max, global_tolerance,
                                std::initializer_list{domain_max / 2.0, domain_max / 3.0}),
                       "increasing and unique");
}

TEST_F(quadrature_stack_seeder_death_tests_t, asserts_on_duplicate_critical_points)
{
    EXPECT_DEBUG_DEATH(sut.seed(stack, subdivider, domain_max, global_tolerance,
                                std::initializer_list{domain_max / 3.0, domain_max / 3.0}),
                       "increasing and unique");
}
} // namespace
} // namespace crv::quadrature
