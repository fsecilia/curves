// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "nast.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <cmath>
#include <limits>

namespace crv::shaping::transitions::generic {
namespace {

struct nast_cache_builder_test_t : Test
{
    struct antiderivative_t
    {
        using scalar_t = float_t;

        int_t segments = 0;

        auto segment_count() const noexcept -> int_t { return segments; }
        auto operator==(antiderivative_t const&) const noexcept -> bool = default;
    };

    using integration_result_t = quadrature::integration_result_t<antiderivative_t>;

    struct mock_integrator_t
    {
        MOCK_METHOD(integration_result_t, call, (float_t tolerance, int_t depth_limit), (const));
    };

    struct integrator_t
    {
        using result_t = integration_result_t;

        mock_integrator_t* mock = nullptr;

        auto operator()(float_t tolerance, int_t depth_limit) const -> result_t
        {
            return mock->call(tolerance, depth_limit);
        }
    };

    StrictMock<mock_integrator_t> mock_integrator;
    nast_cache_builder_t<integrator_t> sut{integrator_t{&mock_integrator}};
};

TEST_F(nast_cache_builder_test_t, accepts_cache_that_meets_requested_tolerance)
{
    auto const integration = integration_result_t{
        .antiderivative = {.segments = 3},
        .achieved_error = 5e-14,
        .max_error = 4e-14,
        .refinement_limited = false,
    };
    EXPECT_CALL(mock_integrator, call(nast_cache_config_t::requested_tolerance, nast_cache_config_t::depth_limit))
        .WillOnce(Return(integration));

    auto const result = sut();

    EXPECT_EQ(result->receipt.segment_count, 3);
}

TEST_F(nast_cache_builder_test_t, rejects_refinement_limited_cache)
{
    auto const integration = integration_result_t{
        .antiderivative = {.segments = 3},
        .achieved_error = 5e-14,
        .max_error = 4e-14,
        .refinement_limited = true,
    };
    EXPECT_CALL(mock_integrator, call(nast_cache_config_t::requested_tolerance, nast_cache_config_t::depth_limit))
        .WillOnce(Return(integration));

    auto const result = sut();

    EXPECT_EQ(result.error(), (nast_cache_error_t{
                                  .requested_tolerance = nast_cache_config_t::requested_tolerance,
                                  .achieved_error = integration.achieved_error,
                                  .max_error = integration.max_error,
                                  .refinement_limited = true,
                              }));
}

TEST_F(nast_cache_builder_test_t, rejects_cache_above_requested_tolerance)
{
    auto const achieved_error = nast_cache_config_t::requested_tolerance * 2;
    auto const integration = integration_result_t{
        .antiderivative = {.segments = 3},
        .achieved_error = achieved_error,
        .max_error = achieved_error,
        .refinement_limited = false,
    };
    EXPECT_CALL(mock_integrator, call(nast_cache_config_t::requested_tolerance, nast_cache_config_t::depth_limit))
        .WillOnce(Return(integration));

    auto const result = sut();

    EXPECT_EQ(result.error(), (nast_cache_error_t{
                                  .requested_tolerance = nast_cache_config_t::requested_tolerance,
                                  .achieved_error = achieved_error,
                                  .max_error = achieved_error,
                                  .refinement_limited = false,
                              }));
}

TEST_F(nast_cache_builder_test_t, rejects_nonfinite_error_receipt)
{
    auto const achieved_error = std::numeric_limits<float_t>::quiet_NaN();
    auto const integration = integration_result_t{
        .antiderivative = {.segments = 3},
        .achieved_error = achieved_error,
        .max_error = 4e-14,
        .refinement_limited = false,
    };
    EXPECT_CALL(mock_integrator, call(nast_cache_config_t::requested_tolerance, nast_cache_config_t::depth_limit))
        .WillOnce(Return(integration));

    auto const result = sut();

    EXPECT_TRUE(std::isnan(result.error().achieved_error));
}

} // namespace
} // namespace crv::shaping::transitions::generic
