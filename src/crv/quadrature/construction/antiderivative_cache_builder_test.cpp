// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "antiderivative_cache_builder.hpp"
#include <crv/test/test.hpp>

namespace crv::quadrature::construction {
namespace {

struct quadrature_antiderivative_cache_builder_test_t : Test
{
    using scalar_t = float_t;
    using sut_t = antiderivative_cache_builder_t<scalar_t, scalar_t>;
    using result_t = sut_t::result_t;
    using cache_t = sut_t::cache_t;

    sut_t sut{};
};

TEST_F(quadrature_antiderivative_cache_builder_test_t, append_none)
{
    auto const actual = std::move(sut).finalize();
    auto const expected = result_t{
        .cache = cache_t{{0.0}, {0.0}},
        .achieved_error = 0.0,
        .max_error = 0.0,
        .refinement_limited = false,
    };

    EXPECT_EQ(expected, actual);
}

TEST_F(quadrature_antiderivative_cache_builder_test_t, append_one)
{
    sut.append(1.3, 5.7, 7.11);
    auto const actual = std::move(sut).finalize();
    auto const expected = result_t{
        .cache = cache_t{{0.0, 1.3}, {0.0, 5.7}},
        .achieved_error = 7.11,
        .max_error = 7.11,
        .refinement_limited = false,
    };

    EXPECT_EQ(expected, actual);
}

TEST_F(quadrature_antiderivative_cache_builder_test_t, append_many)
{
    sut.append(1.3, 5.7, 7.11);
    sut.append(13.17, 17.19, 53.59);
    sut.append(23.29, 31.37, 41.43);

    auto const actual = std::move(sut).finalize();
    auto const expected = result_t{
        .cache = cache_t{{0.0, 1.3, 13.17, 23.29}, {0.0, 5.7, 5.7 + 17.19, 5.7 + 17.19 + 31.37}},
        .achieved_error = 7.11 + 53.59 + 41.43,
        .max_error = 53.59,
        .refinement_limited = false,
    };

    EXPECT_EQ(expected, actual);
}

TEST_F(quadrature_antiderivative_cache_builder_test_t, retains_refinement_limit_diagnostic)
{
    sut.append(1.0, 2.0, 3.0, true);
    sut.append(2.0, 4.0, 5.0, false);

    EXPECT_TRUE(std::move(sut).finalize().refinement_limited);
}

} // namespace
} // namespace crv::quadrature::construction
