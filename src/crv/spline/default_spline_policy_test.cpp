// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "default_spline_policy.hpp"
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/test/test.hpp>
#include <cmath>

namespace crv::spline {
namespace {

namespace production_policy_tests {

using policy_t = default_spline_policy_t<float_t, prod_pipeline_config_t>;
constexpr auto final_layout = policy_t::segment_layout.final;

static_assert(policy_t::domain_end_fixed == policy_t::x_t{policy_t::domain_end});

struct segment_storage_layout_test_t : Test
{
    policy_t::spline_t spline{};
};

TEST_F(segment_storage_layout_test_t, fixed_y_limit_matches_tangent_conversion)
{
    EXPECT_EQ(policy_t::y_limit_fixed, to_fixed<policy_t::y_t>(policy_t::y_limit));
}

TEST_F(segment_storage_layout_test_t, packs_two_segments_per_cache_line)
{
    auto const base = reinterpret_cast<std::uintptr_t>(spline.segments.data());
    auto const first = reinterpret_cast<std::uintptr_t>(&spline.segments[0]);
    auto const second = reinterpret_cast<std::uintptr_t>(&spline.segments[1]);
    auto const third = reinterpret_cast<std::uintptr_t>(&spline.segments[2]);

    EXPECT_EQ(base % 64, 0);
    EXPECT_EQ(second - first, 32);
    EXPECT_EQ(third - first, 64);
}

// final shift [-64, 63] is the negation of aligned exponent [-63, 64].
static_assert(final_layout.min_shift() == -64);
static_assert(final_layout.max_shift() == 63);
static_assert(policy_t::exponent_aligner_t::exponent_min == -63);
static_assert(policy_t::exponent_aligner_t::exponent_max == 64);
static_assert(policy_t::exponent_aligner_t::exponent_min == final_layout.min_exponent());
static_assert(policy_t::exponent_aligner_t::exponent_max == final_layout.max_exponent());

constexpr auto quantized
    = policy_t::segment_quantizer_t{}(policy_t::cubic_t{0.0, 0.0, 0.0, 0.0}, 1e20, policy_t::x_t{1}, policy_t::x_t{0});
static_assert(quantized.b.significand == 48'828'125'000'000'000);
static_assert(quantized.b.shift == -64);
static_assert(-quantized.b.shift == policy_t::exponent_aligner_t::exponent_max);

struct final_b_boundary_test_t : Test
{
    float_t const final_b_first_unencodable_source = std::ldexp(float_t{1}, 67);
};

TEST_F(final_b_boundary_test_t, final_field_boundary_matches_layout_geometry)
{
    auto const final_b_last_encodable_source = std::nextafter(final_b_first_unencodable_source, float_t{0});
    auto const final_b_last_encodable = policy_t::segment_quantizer_t{}(
        policy_t::cubic_t{0.0, 0.0, 0.0, 0.0}, final_b_last_encodable_source, policy_t::x_t{1}, policy_t::x_t{0});
    auto const final_b_first_unencodable = policy_t::segment_quantizer_t{}(
        policy_t::cubic_t{0.0, 0.0, 0.0, 0.0}, final_b_first_unencodable_source, policy_t::x_t{1}, policy_t::x_t{0});

    EXPECT_TRUE(final_layout.is_encodable(final_b_last_encodable.b));
    EXPECT_FALSE(final_layout.is_encodable(final_b_first_unencodable.b));
    EXPECT_EQ(final_b_first_unencodable.b.significand, (int64_t{1} << 56));
    EXPECT_EQ(final_b_first_unencodable.b.shift, -64);
}

TEST_F(final_b_boundary_test_t, segment_factory_rejects_first_unencodable_final_b)
{
    auto const result = policy_t::segment_factory_t{}(
        policy_t::cubic_t{0.0, 0.0, 0.0, 0.0}, final_b_first_unencodable_source, policy_t::x_t{1}, policy_t::x_t{0});

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(),
        (spline_construction_error_t<policy_t::x_t>{
            .reason = spline_construction_error_reason_t::left_endpoint_derivative_not_representable,
            .left = policy_t::x_t{0},
            .right = policy_t::x_t{1},
        }));
}

constexpr auto packed = policy_t::field_packer_t{}(quantized.b, final_layout);
static_assert(packed == 0x56bc75e2d6310040ULL);
constexpr auto unpacked = policy_t::field_unpacker_t{}(packed, final_layout);
static_assert(unpacked == quantized.b);

} // namespace production_policy_tests

} // namespace
} // namespace crv::spline
