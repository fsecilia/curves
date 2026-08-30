// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "nast.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <type_traits>

namespace crv::shaping::transitions {
namespace {

using scalar_t = float_t;

struct value_reference_t
{
    scalar_t u;
    scalar_t expected;
};

struct nast_base_value_test_t : Test, WithParamInterface<value_reference_t>
{
    nast_base_t<scalar_t> sut;
};

TEST_P(nast_base_value_test_t, matches_high_precision_reference)
{
    EXPECT_NEAR(sut.value(GetParam().u), GetParam().expected, 1e-15);
}

INSTANTIATE_TEST_SUITE_P(high_precision_reference, nast_base_value_test_t,
    Values(value_reference_t{0.1, 0.00013789379201631493}, value_reference_t{0.25, 0.06496916912866406},
        value_reference_t{0.5, 0.5}, value_reference_t{0.75, 0.9350308308713359},
        value_reference_t{0.9, 0.9998621062079837}));

struct monotone_interval_t
{
    scalar_t left;
    scalar_t right;
};

struct nast_base_monotonicity_test_t : Test, WithParamInterface<monotone_interval_t>
{
    nast_base_t<scalar_t> sut;
};

TEST_P(nast_base_monotonicity_test_t, value_is_monotone)
{
    EXPECT_LE(sut.value(GetParam().left), sut.value(GetParam().right));
}

INSTANTIATE_TEST_SUITE_P(interior_intervals, nast_base_monotonicity_test_t,
    Values(monotone_interval_t{0.0, 0.01}, monotone_interval_t{0.01, 0.1}, monotone_interval_t{0.1, 0.25},
        monotone_interval_t{0.25, 0.5}, monotone_interval_t{0.5, 0.75}, monotone_interval_t{0.75, 0.9},
        monotone_interval_t{0.9, 0.99}, monotone_interval_t{0.99, 1.0}));

struct symmetry_input_t
{
    scalar_t u;
};

struct nast_base_symmetry_test_t : Test, WithParamInterface<symmetry_input_t>
{
    nast_base_t<scalar_t> sut;
};

TEST_P(nast_base_symmetry_test_t, value_is_symmetric)
{
    auto const u = GetParam().u;
    EXPECT_NEAR(sut.value(1.0 - u), 1.0 - sut.value(u), 1e-15);
}

INSTANTIATE_TEST_SUITE_P(interior_points, nast_base_symmetry_test_t,
    Values(symmetry_input_t{0.01}, symmetry_input_t{0.1}, symmetry_input_t{0.25}, symmetry_input_t{0.4}));

struct nast_base_test_t : Test
{
    nast_base_t<scalar_t> sut;
};

TEST_F(nast_base_test_t, value_below_support_is_exact_zero)
{
    EXPECT_EQ(sut.value(-1.0), 0.0);
}

TEST_F(nast_base_test_t, value_at_support_start_is_exact_zero)
{
    EXPECT_EQ(sut.value(0.0), 0.0);
}

TEST_F(nast_base_test_t, value_at_support_end_is_exact_one)
{
    EXPECT_EQ(sut.value(1.0), 1.0);
}

TEST_F(nast_base_test_t, value_above_support_is_exact_one)
{
    EXPECT_EQ(sut.value(2.0), 1.0);
}

TEST_F(nast_base_test_t, derivative_at_support_start_is_exact_zero)
{
    EXPECT_EQ(sut.derivative(0.0), 0.0);
}

TEST_F(nast_base_test_t, derivative_at_support_end_is_exact_zero)
{
    EXPECT_EQ(sut.derivative(1.0), 0.0);
}

TEST_F(nast_base_test_t, derivative_is_practically_flat_near_support_start)
{
    EXPECT_LT(sut.derivative(0.01), 2e-39);
}

TEST_F(nast_base_test_t, derivative_is_symmetric)
{
    EXPECT_NEAR(sut.derivative(0.25), sut.derivative(0.75), 1e-14);
}

TEST_F(nast_base_test_t, value_jet_uses_analytic_derivative)
{
    auto const u = 0.25;
    auto const tangent = 2.5;
    auto const actual = sut.value(jet_t<scalar_t>{u, tangent});

    EXPECT_EQ(actual.df, sut.derivative(u) * tangent);
}

struct nast_test_t : Test
{
    struct mock_antiderivative_t
    {
        virtual ~mock_antiderivative_t() = default;
        MOCK_METHOD(scalar_t, value, (scalar_t), (const, noexcept));
    };
    StrictMock<mock_antiderivative_t> mock_antiderivative;

    struct antiderivative_t
    {
        mock_antiderivative_t* mock = nullptr;

        [[nodiscard]] auto domain_end() const noexcept -> scalar_t { return scalar_t{0.5}; }
        [[nodiscard]] auto operator()(scalar_t u) const noexcept -> scalar_t { return mock->value(u); }
    };

    using sut_t = nast_t<scalar_t, antiderivative_t>;
    sut_t sut{antiderivative_t{&mock_antiderivative}};
};

static_assert(std::is_copy_constructible_v<nast_test_t::sut_t>);
static_assert(std::is_copy_assignable_v<nast_test_t::sut_t>);
static_assert(std::is_move_constructible_v<nast_test_t::sut_t>);
static_assert(std::is_move_assignable_v<nast_test_t::sut_t>);

TEST_F(nast_test_t, value_uses_nast_base)
{
    EXPECT_NEAR(sut.value(0.25), 0.06496916912866406, 1e-15);
}

TEST_F(nast_test_t, derivative_uses_nast_base)
{
    EXPECT_EQ(sut.derivative(0.25), nast_base_t<scalar_t>{}.derivative(0.25));
}

TEST_F(nast_test_t, antiderivative_uses_retained_lower_half)
{
    EXPECT_CALL(mock_antiderivative, value(0.25)).WillOnce(Return(7.11));

    EXPECT_EQ(sut.antiderivative(0.25), 7.11);
}

TEST_F(nast_test_t, antiderivative_reflects_retained_upper_half)
{
    EXPECT_CALL(mock_antiderivative, value(0.25)).WillOnce(Return(0.0027601852110934675));

    EXPECT_NEAR(sut.antiderivative(0.75), 0.25276018521109347, 1e-15);
}

TEST_F(nast_test_t, antiderivative_below_support_is_exact_zero)
{
    EXPECT_EQ(sut.antiderivative(-1.0), 0.0);
}

TEST_F(nast_test_t, antiderivative_at_support_start_is_exact_zero)
{
    EXPECT_EQ(sut.antiderivative(0.0), 0.0);
}

TEST_F(nast_test_t, antiderivative_at_support_end_is_exact_half)
{
    EXPECT_EQ(sut.antiderivative(1.0), 0.5);
}

TEST_F(nast_test_t, antiderivative_above_support_is_exact_linear_exterior)
{
    EXPECT_EQ(sut.antiderivative(2.0), 1.5);
}

TEST_F(nast_test_t, value_jet_uses_analytic_transition_derivative)
{
    auto const u = 0.25;
    auto const tangent = 2.5;
    auto const actual = sut.value(jet_t<scalar_t>{u, tangent});

    EXPECT_EQ(actual.df, sut.derivative(u) * tangent);
}

TEST_F(nast_test_t, antiderivative_jet_uses_transition_value_as_derivative)
{
    auto const u = 0.75;
    auto const tangent = 2.5;
    EXPECT_CALL(mock_antiderivative, value(0.25)).WillOnce(Return(0.0027601852110934675));
    auto const actual = sut.antiderivative(jet_t<scalar_t>{u, tangent});

    EXPECT_EQ(actual.df, sut.value(u) * tangent);
}

TEST_F(nast_test_t, antiderivative_jet_on_lower_plateau_has_zero_tangent)
{
    EXPECT_EQ(sut.antiderivative(jet_t<scalar_t>{-1.0, 3.0}), (jet_t<scalar_t>{0.0, 0.0}));
}

TEST_F(nast_test_t, antiderivative_jet_on_upper_exterior_has_input_tangent)
{
    EXPECT_EQ(sut.antiderivative(jet_t<scalar_t>{2.0, 3.0}), (jet_t<scalar_t>{1.5, 3.0}));
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG
struct domain_tracking_antiderivative_t
{
    int_t* domain_end_calls;

    [[nodiscard]] auto domain_end() const noexcept -> scalar_t
    {
        ++*domain_end_calls;
        return scalar_t{0.5};
    }

    [[nodiscard]] auto operator()(scalar_t) const noexcept -> scalar_t { return scalar_t{0}; }
};

TEST(shaping_transitions_nast_domain_test_t, antiderivative_evaluation_does_not_requery_validated_domain_end)
{
    auto domain_end_calls = int_t{0};
    auto const sut
        = nast_t<scalar_t, domain_tracking_antiderivative_t>{domain_tracking_antiderivative_t{&domain_end_calls}};
    static_cast<void>(sut.antiderivative(scalar_t{0.25}));
    EXPECT_EQ(domain_end_calls, int_t{1});
}

struct wrong_domain_antiderivative_t
{
    [[nodiscard]] auto domain_end() const noexcept -> scalar_t { return scalar_t{0.25}; }
    [[nodiscard]] auto operator()(scalar_t) const noexcept -> scalar_t { return scalar_t{0}; }
};

TEST(shaping_transitions_nast_domain_test_t, rejects_antiderivative_that_does_not_cover_half_domain)
{
    EXPECT_DEATH(static_cast<void>(nast_t<scalar_t, wrong_domain_antiderivative_t>{wrong_domain_antiderivative_t{}}),
        "half domain");
}
#endif // defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::shaping::transitions
