// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "nast.hpp"
#include <crv/test/test.hpp>
#include <array>
#include <cmath>

namespace crv::shaping::transitions {
namespace {

struct nast_test_t : Test
{
    nast_t::construction_result_t construction = nast_t::make();
    nast_t const* sut = nullptr;

    auto SetUp() -> void override
    {
        ASSERT_TRUE(construction.has_value());
        sut = &*construction;
    }
};

TEST(nast_construction_test_t, production_cache_constructs_successfully)
{
    EXPECT_TRUE(nast_t::make().has_value());
}

TEST_F(nast_test_t, cache_meets_requested_tolerance)
{
    EXPECT_LE(sut->construction_receipt().achieved_error, nast_cache_config_t::requested_tolerance);
}

TEST_F(nast_test_t, cache_is_not_refinement_limited)
{
    EXPECT_FALSE(sut->construction_receipt().refinement_limited);
}

TEST_F(nast_test_t, half_domain_cache_remains_small)
{
    EXPECT_LE(sut->construction_receipt().segment_count, 8);
}

TEST_F(nast_test_t, repeated_construction_reuses_process_cache)
{
    auto const other = nast_t::make();
    ASSERT_TRUE(other.has_value());

    EXPECT_EQ(&sut->construction_receipt(), &other->construction_receipt());
}

struct value_reference_t
{
    float_t u;
    float_t expected;
};

struct nast_value_test_t : TestWithParam<value_reference_t>
{
    nast_t::construction_result_t construction = nast_t::make();
    nast_t const* sut = nullptr;

    auto SetUp() -> void override
    {
        ASSERT_TRUE(construction.has_value());
        sut = &*construction;
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
    float_t u;
    float_t expected;
};

struct nast_antiderivative_test_t : TestWithParam<antiderivative_reference_t>
{
    nast_t::construction_result_t construction = nast_t::make();
    nast_t const* sut = nullptr;

    auto SetUp() -> void override
    {
        ASSERT_TRUE(construction.has_value());
        sut = &*construction;
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
    float_t left;
    float_t right;
};

struct nast_monotonicity_test_t : TestWithParam<monotone_interval_t>
{
    nast_t::construction_result_t construction = nast_t::make();
    nast_t const* sut = nullptr;

    auto SetUp() -> void override
    {
        ASSERT_TRUE(construction.has_value());
        sut = &*construction;
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
    float_t u;
};

struct nast_symmetry_test_t : TestWithParam<symmetry_input_t>
{
    nast_t::construction_result_t construction = nast_t::make();
    nast_t const* sut = nullptr;

    auto SetUp() -> void override
    {
        ASSERT_TRUE(construction.has_value());
        sut = &*construction;
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
    auto const actual = (*sut)(jet_t<float_t>{u, tangent});

    EXPECT_EQ(actual.df, sut->derivative(u) * tangent);
}

TEST_F(nast_test_t, antiderivative_jet_uses_transition_value_as_derivative)
{
    auto const u = 0.75;
    auto const tangent = 2.5;
    auto const actual = sut->antiderivative(jet_t<float_t>{u, tangent});

    EXPECT_EQ(actual.df, (*sut)(u) * tangent);
}

TEST_F(nast_test_t, antiderivative_jet_on_lower_plateau_has_zero_tangent)
{
    EXPECT_EQ(sut->antiderivative(jet_t<float_t>{-1.0, 3.0}), (jet_t<float_t>{0.0, 0.0}));
}

TEST_F(nast_test_t, antiderivative_jet_on_upper_exterior_has_input_tangent)
{
    EXPECT_EQ(sut->antiderivative(jet_t<float_t>{2.0, 3.0}), (jet_t<float_t>{1.5, 3.0}));
}

} // namespace
} // namespace crv::shaping::transitions
