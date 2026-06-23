// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "offset.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/shaping/transitions/smootherstep_integral.hpp>
#include <crv/test/test.hpp>

namespace crv::shaping::transforms {
namespace {

struct test_t
{
    using real_t = float_t;
    using jet_t = jet_t<real_t>;

    // quadratic transition: (x, y, y') in [(0,0,0), (1, 0.5, 1)], gives c1 continuity
    struct transition_t
    {
        template <typename value_t> constexpr auto operator()(value_t input) const noexcept -> value_t
        {
            using scalar_t = scalar_type_t<value_t>;
            auto const t = primal(input);
            if (t <= scalar_t{0}) return value_t{0};
            if (t >= scalar_t{1}) return input - scalar_t{0.5};

            auto const y = t * t / scalar_t{2};
            if constexpr (is_jet<value_t>)
            {
                auto const dt = tangent(input);
                return value_t{y, t * dt};
            }
            else return y;
        }
    };
    static constexpr auto transition = transition_t{};

    using sut_t = offset_t<real_t, transition_t>;
};

// tests identity state
struct identity_test_t : test_t
{
    static constexpr auto sut = sut_t{transition};

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
    static constexpr auto sut = sut_t{start, width, transition};

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
    static constexpr auto sut = sut_t{start, width, transition};

    // horizontal section, x <= start
    static_assert(0.0 == sut(jet_t{0.0, 3.0})); // tangent discarded
    static_assert(0.0 == sut(jet_t{1.5, 1.0}));

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

TEST_F(shaping_transforms_offset_death_tests_t, invariant_start_must_be_0_when_width_is_0)
{
    EXPECT_DEATH((sut_t(1.0, 0.0, transition)), "invariant violated");
}

TEST_F(shaping_transforms_offset_death_tests_t, negative_start)
{
    EXPECT_DEATH((sut_t(-1.0, 1.0, transition)), "invariant violated");
}

TEST_F(shaping_transforms_offset_death_tests_t, negative_width)
{
    EXPECT_DEATH((sut_t(0.0, -1.0, transition)), "invariant violated");
}

#endif

} // namespace
} // namespace crv::shaping::transforms
