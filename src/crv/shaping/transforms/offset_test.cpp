// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "offset.hpp"
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

    using sut_t = offset_t<real_t, transition_t>;

    static constexpr auto min_width = 1.0;
};

// tests identity state
struct identity_test_t : test_t
{
    static constexpr auto sut = sut_t{0.0, 0.0, min_width, transition};

    static_assert(-10.0 == sut(-10.0));
    static_assert(0.0 == sut(0.0));
    static_assert(1e-5 == sut(1e-5));
    static_assert(10.0 == sut(10.0));

    static_assert(sut(jet_t{10.0, 3.0}) == jet_t{10.0, 3.0});
};

// tests pure transition with no horizontal section
struct transition_only_test_t : test_t
{
    static constexpr auto start = 0.0;
    static constexpr auto width = 2.0;
    static constexpr auto sut = sut_t{start, width, min_width, transition};

    // horizontal section, x <= start
    static_assert(jet_t{0.0, 0.0} == sut(jet_t{-1.0, 3.0}));
    static_assert(jet_t{0.0, 0.0} == sut(jet_t{0.0, 1.0}));

    // transition section
    static_assert(jet_t{0.0625, 0.25} == sut(jet_t{0.5, 1.0}));
    static_assert(jet_t{0.0625, 0.5} == sut(jet_t{0.5, 2.0}));
    static_assert(jet_t{0.25, 0.5} == sut(jet_t{1.0, 1.0}));

    // lagged original section, x >= start + width
    static_assert(jet_t{1.0, 1.0} == sut(jet_t{2.0, 1.0}));
    static_assert(jet_t{4.0, 2.0} == sut(jet_t{5.0, 2.0}));

    // scalars
    static_assert(0.0 == sut(0.0));
    static_assert(0.0625 == sut(0.5));
    static_assert(1.0 == sut(2.0));
};

struct transition_with_horizontal_test_t : test_t
{
    static constexpr auto start = 1.5;
    static constexpr auto width = 2.0;
    static constexpr auto sut = sut_t{start, width, min_width, transition};

    // horizontal section, x <= start
    static_assert(jet_t{0.0, 0.0} == sut(jet_t{0.0, 3.0}));
    static_assert(jet_t{0.0, 0.0} == sut(jet_t{1.5, 1.0}));

    // transition section
    static_assert(jet_t{0.0625, 0.25} == sut(jet_t{2.0, 1.0}));
    static_assert(jet_t{0.0625, 0.5} == sut(jet_t{2.0, 2.0}));
    static_assert(jet_t{0.25, 0.5} == sut(jet_t{2.5, 1.0}));

    // lagged original section, x >= start + width
    static_assert(jet_t{1.0, 1.0} == sut(jet_t{3.5, 1.0}));
    static_assert(jet_t{2.5, 2.0} == sut(jet_t{5.0, 2.0}));

    // scalars
    static_assert(0.0 == sut(0.0));
    static_assert(0.0625 == sut(2.0));
    static_assert(1.0 == sut(3.5));
};

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct shaping_transforms_offset_death_tests_t : test_t, Test
{};

TEST_F(shaping_transforms_offset_death_tests_t, zero_width_nonzero_start)
{
    EXPECT_DEATH((sut_t{1.0, 0.0, min_width, transition}), "invariant violated");
}

TEST_F(shaping_transforms_offset_death_tests_t, negative_start)
{
    EXPECT_DEATH((sut_t{-1.0, min_width, min_width, transition}), "invariant violated");
}

TEST_F(shaping_transforms_offset_death_tests_t, below_min_width)
{
    // this constructs safely
    [[maybe_unused]] auto working_sut = sut_t{0.0, min_width, min_width, transition};

    // decrementing width by one float does not construct
    auto const just_before_min_width = std::nextafter(min_width, 0.0);
    EXPECT_DEATH((sut_t{0.0, just_before_min_width, min_width, transition}), "invariant violated");
}

TEST_F(shaping_transforms_offset_death_tests_t, negative_min_width)
{
    EXPECT_DEATH((sut_t{0.0, min_width, -min_width, transition}), "invariant violated");
}

TEST_F(shaping_transforms_offset_death_tests_t, zero_min_width)
{
    EXPECT_DEATH((sut_t{1.0, 1.0, 0.0, transition}), "invariant violated");
}

#endif

} // namespace
} // namespace crv::shaping::transforms
