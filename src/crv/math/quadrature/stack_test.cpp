// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stack.hpp"
#include <crv/ranges.hpp>
#include <crv/test/test.hpp>
#include <initializer_list>

namespace crv::quadrature {
namespace {

using real_t = float_t;

struct common_fixture_t : Test
{
    using segment_t   = segment_t<real_t>;
    using bisection_t = bisection_t<real_t>;

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

    auto expect_order(std::initializer_list<int_t> ids, auto& sut) -> void
    {
        for (auto id : ids | std::views::reverse) { EXPECT_EQ(id, sut.pop().depth); }
    }
};

// ====================================================================================================================
// stack_t
// ====================================================================================================================

struct quadrature_stack_test_t : common_fixture_t
{
    using sut_t = stack_t<real_t>;
    sut_t sut{};

    auto expect_order(std::initializer_list<int_t> ids) -> void
    {
        return common_fixture_t::expect_order(std::move(ids), sut);
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

} // namespace
} // namespace crv::quadrature
