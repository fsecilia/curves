// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "output_scale.hpp"
#include <crv/test/test.hpp>
#include <limits>
#include <utility>

namespace crv::shaping::transforms {
namespace {

using scalar_t = float_t;
using jet_t = crv::jet_t<scalar_t>;
using sut_t = output_scale_t<scalar_t>;

[[nodiscard]] auto make_sut(scalar_t scale) -> sut_t
{
    return std::move(sut_t::make(scale)).value();
}

TEST(shaping_transforms_output_scale_test_t, scales_scalar)
{
    EXPECT_EQ(make_sut(2.5).apply(4.0), 10.0);
}

TEST(shaping_transforms_output_scale_test_t, identity_scale_leaves_scalar_unchanged)
{
    EXPECT_EQ(make_sut(1.0).apply(4.0), 4.0);
}

TEST(shaping_transforms_output_scale_test_t, fractional_scale_contracts_scalar)
{
    EXPECT_EQ(make_sut(0.5).apply(4.0), 2.0);
}

TEST(shaping_transforms_output_scale_test_t, scales_jet_primal_and_tangent)
{
    EXPECT_EQ(make_sut(2.0).apply(jet_t{3.0, 5.0}), (jet_t{6.0, 10.0}));
}

TEST(shaping_transforms_output_scale_test_t, rejects_zero_scale)
{
    EXPECT_EQ(sut_t::make(0.0), std::unexpected{output_scale_error_t::scale_not_positive});
}

TEST(shaping_transforms_output_scale_test_t, rejects_negative_scale)
{
    EXPECT_EQ(sut_t::make(-1.0), std::unexpected{output_scale_error_t::scale_not_positive});
}

TEST(shaping_transforms_output_scale_test_t, rejects_nonfinite_scale)
{
    EXPECT_EQ(sut_t::make(std::numeric_limits<scalar_t>::infinity()),
        std::unexpected{output_scale_error_t::scale_not_finite});
}

TEST(shaping_transforms_output_scale_test_t, accepts_exact_positive_representability_boundary)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    EXPECT_EQ(make_sut(2.0).try_apply(max / 2.0), max);
}

TEST(shaping_transforms_output_scale_test_t, rejects_positive_output_beyond_representability_boundary)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const boundary = max / 2.0;
    EXPECT_FALSE(make_sut(2.0).try_apply(std::nextafter(boundary, max)));
}

TEST(shaping_transforms_output_scale_test_t, rejects_negative_output_beyond_representability_boundary)
{
    auto const lowest = std::numeric_limits<scalar_t>::lowest();
    auto const boundary = lowest / 2.0;
    EXPECT_FALSE(make_sut(2.0).try_apply(std::nextafter(boundary, lowest)));
}

} // namespace
} // namespace crv::shaping::transforms
