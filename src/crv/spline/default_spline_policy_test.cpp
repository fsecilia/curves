// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "default_spline_policy.hpp"
#include <crv/spline/pipeline_config.hpp>

namespace crv::spline {
namespace {

namespace production_policy_tests {

using policy_t = default_spline_policy_t<float_t, prod_pipeline_config_t>;
constexpr auto final_layout = policy_t::segment_layout.final;

// final shift [-64, 63] is the negation of aligned exponent [-63, 64].
static_assert(final_layout.min_shift() == -64);
static_assert(final_layout.max_shift() == 63);
static_assert(policy_t::exponent_aligner_t::exponent_min == -63);
static_assert(policy_t::exponent_aligner_t::exponent_max == 64);
static_assert(policy_t::exponent_aligner_t::exponent_min == final_layout.min_exponent());
static_assert(policy_t::exponent_aligner_t::exponent_max == final_layout.max_exponent());

constexpr auto quantized
    = policy_t::segment_quantizer_t{}(policy_t::cubic_t{0.0, 0.0, 1e20, 0.0}, policy_t::x_t{1}, policy_t::x_t{0});
static_assert(quantized.b.significand == 48'828'125'000'000'000);
static_assert(quantized.b.shift == -64);
static_assert(-quantized.b.shift == policy_t::exponent_aligner_t::exponent_max);

constexpr auto packed = policy_t::field_packer_t{}(quantized.b, final_layout);
static_assert(packed == 0x56bc75e2d6310040ULL);
constexpr auto unpacked = policy_t::field_unpacker_t{}(packed, final_layout);
static_assert(unpacked == quantized.b);

} // namespace production_policy_tests

} // namespace
} // namespace crv::spline
