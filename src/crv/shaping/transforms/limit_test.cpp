// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "limit.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/shaping/transitions/quadratic.hpp>
#include <crv/test/test.hpp>

namespace crv::shaping::transforms {
namespace {

struct test_t
{
    using real_t = float_t;
    using jet_t = jet_t<real_t>;

    using transition_t = transitions::quadratic_t;
    static constexpr auto transition = transition_t{};

    using sut_t = limit_t<real_t, transition_t>;
};

// tests identity state, no engagement
struct identity_test_t : test_t
{
    static constexpr auto sut = sut_t{0.0, 2.0, 0.0, transition};

    static_assert(-10.0 == sut(-10.0));
    static_assert(0.0 == sut(0.0));
    static_assert(1e-5 == sut(1e-5));
    static_assert(10.0 == sut(10.0));

    static_assert(sut(jet_t{10.0, 3.0}) == jet_t{10.0, 3.0});
};

// tests with zero start, full engagement
struct transition_only_test_t : test_t
{
    static constexpr auto start = 0.0;
    static constexpr auto width = 2.0;
    static constexpr auto blend = 1.0;
    static constexpr auto sut = sut_t{start, width, blend, transition};

    // original section, x <= start
    static_assert(jet_t{-1.0, 3.0} == sut(jet_t{-1.0, 3.0}));
    static_assert(jet_t{0.0, 1.0} == sut(jet_t{0.0, 1.0}));

    // transition section
    static_assert(jet_t{0.4375, 0.75} == sut(jet_t{0.5, 1.0}));
    static_assert(jet_t{0.4375, 1.5} == sut(jet_t{0.5, 2.0}));
    static_assert(jet_t{0.75, 0.5} == sut(jet_t{1.0, 1.0}));

    // limited section, x >= start + width
    static_assert(jet_t{1.0, 0.0} == sut(jet_t{2.0, 1.0}));
    static_assert(jet_t{1.0, 0.0} == sut(jet_t{5.0, 2.0}));

    // scalars
    static_assert(0.0 == sut(0.0));
    static_assert(0.75 == sut(1.0));
    static_assert(1.0 == sut(2.0));
};

// tests with nonzero start, full engagement
struct transition_with_start_test_t : test_t
{
    static constexpr auto start = 1.5;
    static constexpr auto width = 2.0;
    static constexpr auto blend = 1.0;
    static constexpr auto sut = sut_t{start, width, blend, transition};

    // original section, x <= start
    static_assert(jet_t{0.0, 3.0} == sut(jet_t{0.0, 3.0}));
    static_assert(jet_t{1.5, 1.0} == sut(jet_t{1.5, 1.0}));

    // transition section
    static_assert(jet_t{1.9375, 0.75} == sut(jet_t{2.0, 1.0}));
    static_assert(jet_t{1.9375, 1.5} == sut(jet_t{2.0, 2.0})); // same primal, doubled dual
    static_assert(jet_t{2.25, 0.5} == sut(jet_t{2.5, 1.0}));

    // limited section, x >= start + width
    static_assert(jet_t{2.5, 0.0} == sut(jet_t{3.5, 1.0}));
    static_assert(jet_t{2.5, 0.0} == sut(jet_t{5.0, 2.0}));

    // scalars
    static_assert(0.0 == sut(0.0));
    static_assert(1.9375 == sut(2.0));
    static_assert(2.5 == sut(3.5));
};

// tests zero start, partial engagement
struct transition_only_with_blend_test_t : test_t
{
    static constexpr auto start = 0.0;
    static constexpr auto width = 2.0;
    static constexpr auto blend = 0.5;
    static constexpr auto sut = sut_t{start, width, blend, transition};

    // original section, x <= start
    static_assert(jet_t{0.0, 1.0} == sut(jet_t{0.0, 1.0}));

    // transition section
    static_assert(jet_t{0.875, 0.75} == sut(jet_t{1.0, 1.0}));

    // limited section, x >= start + width; blended here, so it still grows, but at reduced rate
    static_assert(jet_t{1.5, 0.5} == sut(jet_t{2.0, 1.0}));
    static_assert(jet_t{3.0, 0.5} == sut(jet_t{5.0, 1.0}));

    // scalars
    static_assert(0.0 == sut(0.0));
    static_assert(0.875 == sut(1.0));
    static_assert(1.5 == sut(2.0));
    static_assert(3.0 == sut(5.0));
};

// tests nonzero start, partial engagement
struct transition_with_start_and_blend_test_t : test_t
{
    static constexpr auto start = 1.5;
    static constexpr auto width = 2.0;
    static constexpr auto blend = 0.5;
    static constexpr auto sut = sut_t{start, width, blend, transition};

    // original section, x <= start — blend
    static_assert(jet_t{0.0, 3.0} == sut(jet_t{0.0, 3.0}));

    // transition section
    static_assert(jet_t{1.96875, 0.875} == sut(jet_t{2.0, 1.0}));

    // limited section, x >= start + width
    static_assert(jet_t{3.75, 0.5} == sut(jet_t{5.0, 1.0}));

    // scalars
    static_assert(0.0 == sut(0.0));
    static_assert(3.75 == sut(5.0));
};

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct shaping_transforms_limit_death_tests_t : test_t, Test
{};

TEST_F(shaping_transforms_limit_death_tests_t, negative_width)
{
    EXPECT_DEATH((sut_t(0.0, -1.0, 1.0, transition)), "width.*0");
}

TEST_F(shaping_transforms_limit_death_tests_t, zero_width)
{
    EXPECT_DEATH((sut_t(0.0, 0.0, 1.0, transition)), "width.*0");
}

TEST_F(shaping_transforms_limit_death_tests_t, negative_blend)
{
    EXPECT_DEATH((sut_t(0.0, 1.0, -0.1, transition)), "blend.*in.*0, 1");
}

TEST_F(shaping_transforms_limit_death_tests_t, excess_blend)
{
    EXPECT_DEATH((sut_t(0.0, 1.0, 1.1, transition)), "blend.*in.*0, 1");
}

#endif

} // namespace
} // namespace crv::shaping::transforms
