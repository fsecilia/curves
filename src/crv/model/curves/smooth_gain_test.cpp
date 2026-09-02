// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "smooth_gain.hpp"
#include <crv/model/curves/concepts.hpp>
#include <crv/model/curves/test.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>
#include <string_view>

namespace crv::model::curves {
namespace {

using real_t = float_t;
using params_t = smooth_gain_t::params_t<real_t>;
using evaluator_t = smooth_gain_t::evaluator_t<real_t>;
using continuity_t = shaping::transitions::continuity_t;

static_assert(is_curve<evaluator_t, real_t>);

struct transition_vector_t
{
    std::string_view name;
    continuity_t continuity;
    real_t value_at_quarter;
    real_t derivative_at_quarter;

    friend auto operator<<(std::ostream& out, transition_vector_t const& src) -> std::ostream&
    {
        return out << src.name;
    }
};

transition_vector_t const transition_vectors[] = {
    {"c1", continuity_t::c1, 0.15625, 1.125},
    {"c2", continuity_t::c2, 0.103515625, 1.0546875},
    {"c3", continuity_t::c3, 0.070556640625, 0.9228515625},
    {"cinfinity", continuity_t::cinfinity, 0.06496916912866404, 1.079967576735913},
};

struct smooth_gain_transition_test_t : TestWithParam<transition_vector_t>
{
    static constexpr auto v_0 = real_t{2};
    static constexpr auto v_1 = real_t{6};
    static constexpr auto g_t = real_t{0.5};
    static constexpr auto g_f = real_t{8};
    static constexpr auto span = v_1 - v_0;
    static constexpr auto x_quarter = v_0 + span / real_t{4};
    static constexpr auto tangent = real_t{1.3};
    static constexpr auto tolerance = real_t{1e-12};

    transition_vector_t const& vector = GetParam();
    evaluator_t sut{params_t{v_0, v_1, g_t, g_f, vector.continuity}};

    auto expected_value() const -> real_t
    {
        return std::exp(std::log(g_t) + (std::log(g_f) - std::log(g_t)) * vector.value_at_quarter);
    }
};

TEST_P(smooth_gain_transition_test_t, selects_transition_value)
{
    EXPECT_NEAR(sut(x_quarter), expected_value(), tolerance);
}

TEST_P(smooth_gain_transition_test_t, propagates_transition_derivative)
{
    auto const actual = sut(jet_t<real_t>{x_quarter, tangent}).df;
    auto const expected
        = tangent * expected_value() * (std::log(g_f) - std::log(g_t)) * vector.derivative_at_quarter / span;

    EXPECT_NEAR(actual, expected, tolerance);
}

INSTANTIATE_TEST_SUITE_P(transitions, smooth_gain_transition_test_t, ValuesIn(transition_vectors),
    test_name_generator_t<transition_vector_t>{});

struct smooth_gain_test_t : Test
{
    static constexpr auto v_0 = real_t{2};
    static constexpr auto v_1 = real_t{6};
    static constexpr auto g_t = real_t{0.5};
    static constexpr auto g_f = real_t{8};

    evaluator_t sut{params_t{v_0, v_1, g_t, g_f, continuity_t::cinfinity}};
};

TEST_F(smooth_gain_test_t, returns_tracking_gain_before_transition)
{
    EXPECT_EQ(sut(real_t{1}), g_t);
}

TEST_F(smooth_gain_test_t, returns_tracking_gain_at_transition_start)
{
    EXPECT_EQ(sut(v_0), g_t);
}

TEST_F(smooth_gain_test_t, returns_final_gain_at_transition_end)
{
    EXPECT_EQ(sut(v_1), g_f);
}

TEST_F(smooth_gain_test_t, returns_final_gain_after_transition)
{
    EXPECT_EQ(sut(real_t{7}), g_f);
}

TEST_F(smooth_gain_test_t, derivative_is_zero_at_transition_start)
{
    EXPECT_EQ(sut(jet_t<real_t>{v_0, real_t{1}}).df, real_t{0});
}

TEST_F(smooth_gain_test_t, derivative_is_zero_at_transition_end)
{
    EXPECT_EQ(sut(jet_t<real_t>{v_1, real_t{1}}).df, real_t{0});
}

TEST_F(smooth_gain_test_t, input_domain_starts_at_zero)
{
    EXPECT_EQ(sut.input_domain().first(), real_t{0});
}

TEST_F(smooth_gain_test_t, input_domain_ends_at_largest_finite_value)
{
    EXPECT_EQ(sut.input_domain().last(), std::numeric_limits<real_t>::max());
}

TEST_F(smooth_gain_test_t, exposes_transition_support_as_critical_points)
{
    EXPECT_EQ(sut.critical_points(), (std::vector<real_t>{v_0, v_1}));
}

TEST(smooth_gain_adapter_test_t, config_converts_to_params)
{
    auto const config = smooth_gain_t::config_t{
        .v_0{"v_0", -1.0},
        .v_1{"v_1", 12.0},
        .g_t{"g_t", 0.75},
        .g_f{"g_f", 3.0},
        .transition{"transition", continuity_t::c2},
    };

    EXPECT_EQ(to_params<real_t>(config), (params_t{-1.0, 12.0, 0.75, 3.0, continuity_t::c2}));
}

} // namespace
} // namespace crv::model::curves
