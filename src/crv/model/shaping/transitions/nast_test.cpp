// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "nast.hpp"
#include <crv/model/shaping/transitions/construction/nast_builder.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <cmath>

namespace crv::shaping::transitions {
namespace {

using scalar_t = float_t;
using transition_t = nast_t<scalar_t>;
using builder_t = construction::nast_builder_t<scalar_t>;

struct builder_factory_t
{
    auto operator()() const -> builder_t
    {
        return builder_t{builder_t::integrator_t{builder_t::requested_tolerance, builder_t::depth_limit}};
    }
};

struct nast_test_t : Test
{
    builder_t::result_t construction = builder_factory_t{}()();
    transition_t const* sut = nullptr;

    auto SetUp() -> void override
    {
        ASSERT_TRUE(construction.has_value());
        sut = &construction->transition;
    }
};

struct value_reference_t
{
    scalar_t u;
    scalar_t expected;
};

struct nast_value_test_t : TestWithParam<value_reference_t>
{
    builder_t::result_t construction = builder_factory_t{}()();
    transition_t const* sut = nullptr;

    auto SetUp() -> void override
    {
        ASSERT_TRUE(construction.has_value());
        sut = &construction->transition;
    }
};

TEST_P(nast_value_test_t, matches_high_precision_reference)
{
    EXPECT_NEAR((*sut)(GetParam().u), GetParam().expected, 1e-15);
}

INSTANTIATE_TEST_SUITE_P(high_precision_reference, nast_value_test_t,
    Values(value_reference_t{0.1, 0.00013789379201631493}, value_reference_t{0.25, 0.06496916912866406},
        value_reference_t{0.5, 0.5}, value_reference_t{0.75, 0.9350308308713359},
        value_reference_t{0.9, 0.9998621062079837}));

struct antiderivative_reference_t
{
    scalar_t u;
    scalar_t expected;
};

struct nast_antiderivative_test_t : TestWithParam<antiderivative_reference_t>
{
    builder_t::result_t construction = builder_factory_t{}()();
    transition_t const* sut = nullptr;

    auto SetUp() -> void override
    {
        ASSERT_TRUE(construction.has_value());
        sut = &construction->transition;
    }
};

TEST_P(nast_antiderivative_test_t, matches_high_precision_reference)
{
    EXPECT_NEAR(sut->antiderivative(GetParam().u), GetParam().expected, 2e-14);
}

INSTANTIATE_TEST_SUITE_P(high_precision_reference, nast_antiderivative_test_t,
    Values(antiderivative_reference_t{0.1, 0.0000011531140741478112},
        antiderivative_reference_t{0.25, 0.0027601852110934675},
        antiderivative_reference_t{0.5, 0.0688874741344636},
        antiderivative_reference_t{0.75, 0.25276018521109347},
        antiderivative_reference_t{0.9, 0.40000115311407415}));

struct monotone_interval_t
{
    scalar_t left;
    scalar_t right;
};

struct nast_monotonicity_test_t : TestWithParam<monotone_interval_t>
{
    builder_t::result_t construction = builder_factory_t{}()();
    transition_t const* sut = nullptr;

    auto SetUp() -> void override
    {
        ASSERT_TRUE(construction.has_value());
        sut = &construction->transition;
    }
};

TEST_P(nast_monotonicity_test_t, value_is_monotone)
{
    EXPECT_LE((*sut)(GetParam().left), (*sut)(GetParam().right));
}

INSTANTIATE_TEST_SUITE_P(interior_intervals, nast_monotonicity_test_t,
    Values(monotone_interval_t{0.0, 0.01}, monotone_interval_t{0.01, 0.1}, monotone_interval_t{0.1, 0.25},
        monotone_interval_t{0.25, 0.5}, monotone_interval_t{0.5, 0.75}, monotone_interval_t{0.75, 0.9},
        monotone_interval_t{0.9, 0.99}, monotone_interval_t{0.99, 1.0}));

struct symmetry_input_t
{
    scalar_t u;
};

struct nast_symmetry_test_t : TestWithParam<symmetry_input_t>
{
    builder_t::result_t construction = builder_factory_t{}()();
    transition_t const* sut = nullptr;

    auto SetUp() -> void override
    {
        ASSERT_TRUE(construction.has_value());
        sut = &construction->transition;
    }
};

TEST_P(nast_symmetry_test_t, value_is_symmetric)
{
    auto const u = GetParam().u;
    EXPECT_NEAR((*sut)(1.0 - u), 1.0 - (*sut)(u), 1e-15);
}

INSTANTIATE_TEST_SUITE_P(interior_points, nast_symmetry_test_t,
    Values(symmetry_input_t{0.01}, symmetry_input_t{0.1}, symmetry_input_t{0.25}, symmetry_input_t{0.4}));

TEST_F(nast_test_t, value_below_support_is_exact_zero)
{
    EXPECT_EQ((*sut)(-1.0), 0.0);
}

TEST_F(nast_test_t, value_at_support_start_is_exact_zero)
{
    EXPECT_EQ((*sut)(0.0), 0.0);
}

TEST_F(nast_test_t, value_at_support_end_is_exact_one)
{
    EXPECT_EQ((*sut)(1.0), 1.0);
}

TEST_F(nast_test_t, value_above_support_is_exact_one)
{
    EXPECT_EQ((*sut)(2.0), 1.0);
}

TEST_F(nast_test_t, derivative_at_support_start_is_exact_zero)
{
    EXPECT_EQ(sut->derivative(0.0), 0.0);
}

TEST_F(nast_test_t, derivative_at_support_end_is_exact_zero)
{
    EXPECT_EQ(sut->derivative(1.0), 0.0);
}

TEST_F(nast_test_t, derivative_is_practically_flat_near_support_start)
{
    EXPECT_LT(sut->derivative(0.01), 2e-39);
}

TEST_F(nast_test_t, derivative_is_symmetric)
{
    EXPECT_NEAR(sut->derivative(0.25), sut->derivative(0.75), 1e-14);
}

TEST_F(nast_test_t, antiderivative_below_support_is_exact_zero)
{
    EXPECT_EQ(sut->antiderivative(-1.0), 0.0);
}

TEST_F(nast_test_t, antiderivative_at_support_start_is_exact_zero)
{
    EXPECT_EQ(sut->antiderivative(0.0), 0.0);
}

TEST_F(nast_test_t, antiderivative_at_support_end_is_exact_half)
{
    EXPECT_EQ(sut->antiderivative(1.0), 0.5);
}

TEST_F(nast_test_t, antiderivative_above_support_is_exact_linear_exterior)
{
    EXPECT_EQ(sut->antiderivative(2.0), 1.5);
}

TEST_F(nast_test_t, value_jet_uses_analytic_transition_derivative)
{
    auto const u = 0.25;
    auto const tangent = 2.5;
    auto const actual = (*sut)(jet_t<scalar_t>{u, tangent});

    EXPECT_EQ(actual.df, sut->derivative(u) * tangent);
}

TEST_F(nast_test_t, antiderivative_jet_uses_transition_value_as_derivative)
{
    auto const u = 0.75;
    auto const tangent = 2.5;
    auto const actual = sut->antiderivative(jet_t<scalar_t>{u, tangent});

    EXPECT_EQ(actual.df, (*sut)(u) * tangent);
}

TEST_F(nast_test_t, antiderivative_jet_on_lower_plateau_has_zero_tangent)
{
    EXPECT_EQ(sut->antiderivative(jet_t<scalar_t>{-1.0, 3.0}), (jet_t<scalar_t>{0.0, 0.0}));
}

TEST_F(nast_test_t, antiderivative_jet_on_upper_exterior_has_input_tangent)
{
    EXPECT_EQ(sut->antiderivative(jet_t<scalar_t>{2.0, 3.0}), (jet_t<scalar_t>{1.5, 3.0}));
}

} // namespace
} // namespace crv::shaping::transitions
