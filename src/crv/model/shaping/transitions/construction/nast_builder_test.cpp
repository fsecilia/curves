// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "nast_builder.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <cmath>
#include <limits>
#include <utility>

namespace crv::shaping::transitions::construction {
namespace {

struct nast_builder_test_t : Test
{
    using scalar_t = float_t;

    struct antiderivative_t
    {
        int_t segments = 0;

        auto segment_count() const noexcept -> int_t { return segments; }
        auto operator==(antiderivative_t const&) const noexcept -> bool = default;
    };

    struct integration_result_t
    {
        antiderivative_t antiderivative;
        scalar_t achieved_error;
        scalar_t max_error;
        bool refinement_limited;
    };

    struct mock_integrator_t
    {
        virtual ~mock_integrator_t() = default;
        MOCK_METHOD(integration_result_t, call, (scalar_t domain_end), (const));
    };
    StrictMock<mock_integrator_t> mock_integrator;

    struct integrator_t
    {
        mock_integrator_t* mock = nullptr;

        auto operator()(transitions::detail::nast_integral_t<scalar_t>, scalar_t domain_end,
            std::array<scalar_t, 0> const&) const -> integration_result_t
        {
            return mock->call(domain_end);
        }
    };

    struct transition_t
    {
        antiderivative_t antiderivative;

        explicit transition_t(antiderivative_t value) noexcept : antiderivative{std::move(value)} {}
    };

    using sut_t = nast_builder_t<scalar_t, integrator_t, transition_t>;
    sut_t sut{integrator_t{&mock_integrator}};

    auto expect_integration(integration_result_t integration) -> void
    {
        EXPECT_CALL(mock_integrator, call(sut_t::domain_end)).WillOnce(Return(std::move(integration)));
    }
};

TEST_F(nast_builder_test_t, accepts_integration_that_meets_requested_tolerance)
{
    expect_integration({
        .antiderivative = {.segments = 3},
        .achieved_error = 5e-14,
        .max_error = 4e-14,
        .refinement_limited = false,
    });

    auto const result = sut();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->transition.antiderivative.segments, 3);
}

TEST_F(nast_builder_test_t, receipt_retains_segment_count)
{
    expect_integration({
        .antiderivative = {.segments = 3},
        .achieved_error = 5e-14,
        .max_error = 4e-14,
        .refinement_limited = false,
    });

    auto const result = sut();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->receipt.segment_count, 3);
}

TEST_F(nast_builder_test_t, rejects_refinement_limited_integration)
{
    auto const integration = integration_result_t{
        .antiderivative = {.segments = 3},
        .achieved_error = 5e-14,
        .max_error = 4e-14,
        .refinement_limited = true,
    };
    expect_integration(integration);

    auto const result = sut();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), (sut_t::error_t{
                                  .requested_tolerance = sut_t::requested_tolerance,
                                  .achieved_error = integration.achieved_error,
                                  .max_error = integration.max_error,
                                  .refinement_limited = true,
                              }));
}

TEST_F(nast_builder_test_t, rejects_integration_above_requested_tolerance)
{
    auto const achieved_error = sut_t::requested_tolerance * 2;
    auto const integration = integration_result_t{
        .antiderivative = {.segments = 3},
        .achieved_error = achieved_error,
        .max_error = achieved_error,
        .refinement_limited = false,
    };
    expect_integration(integration);

    auto const result = sut();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), (sut_t::error_t{
                                  .requested_tolerance = sut_t::requested_tolerance,
                                  .achieved_error = achieved_error,
                                  .max_error = achieved_error,
                                  .refinement_limited = false,
                              }));
}

TEST_F(nast_builder_test_t, rejects_nonfinite_error_receipt)
{
    auto const achieved_error = std::numeric_limits<nast_builder_test_t::scalar_t>::quiet_NaN();
    expect_integration({
        .antiderivative = {.segments = 3},
        .achieved_error = achieved_error,
        .max_error = 4e-14,
        .refinement_limited = false,
    });

    auto const result = sut();

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::isnan(result.error().achieved_error));
}

struct nast_builder_production_test_t : Test
{
    using scalar_t = float_t;
    using sut_t = nast_builder_t<scalar_t>;

    sut_t sut{sut_t::integrator_t{sut_t::requested_tolerance, sut_t::depth_limit}};
    sut_t::result_t result = sut();

    auto SetUp() -> void override { ASSERT_TRUE(result.has_value()); }
};

TEST(nast_builder_production_construction_test_t, constructs_transition)
{
    using sut_t = nast_builder_t<float_t>;
    auto sut = sut_t{sut_t::integrator_t{sut_t::requested_tolerance, sut_t::depth_limit}};
    EXPECT_TRUE(sut().has_value());
}

TEST_F(nast_builder_production_test_t, cache_meets_requested_tolerance)
{
    EXPECT_LE(result->receipt.achieved_error, sut_t::requested_tolerance);
}

TEST_F(nast_builder_production_test_t, cache_is_not_refinement_limited)
{
    EXPECT_FALSE(result->receipt.refinement_limited);
}

TEST_F(nast_builder_production_test_t, half_domain_cache_remains_small)
{
    EXPECT_LE(result->receipt.segment_count, 8);
}

} // namespace
} // namespace crv::shaping::transitions::construction
