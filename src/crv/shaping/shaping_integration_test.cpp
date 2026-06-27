// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/test/test.hpp>

namespace crv::shaping {
namespace {

/// composes two functions: outer(inner(value)) -> value_t
template <typename outer_t, typename inner_t> struct composite_transform_t
{
    outer_t outer;
    inner_t inner;

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t value) const noexcept -> value_t
    {
        return outer(inner(value));
    }
};

/// composes curve with input and output transforms: out(curve(in(value))) -> value_t
template <typename input_t, typename curve_t, typename output_t> struct composite_curve_t
{
    input_t in;
    curve_t curve;
    output_t out;

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        return out(curve(in(input)));
    }
};

//
// 2. Dumb Transforms & Configs (For PoC)
//

// Input Transforms
struct dummy_in_add_t
{
    double v;
    constexpr auto operator()(double x) const noexcept -> double { return x + v; }
};
struct dummy_in_mul_t
{
    double v;
    constexpr auto operator()(double x) const noexcept -> double { return x * v; }
};

// Output Transforms
struct dummy_out_mul_t
{
    double v;
    constexpr auto operator()(double x) const noexcept -> double { return x * v; }
};
struct dummy_out_add_t
{
    double v;
    constexpr auto operator()(double x) const noexcept -> double { return x + v; }
};

// Curve Config
struct dummy_curve_config_t
{
    double cusp_anchor; // The parameter we need to quantize
};

// Curve
struct dummy_curve_t
{
    dummy_curve_config_t config;

    // just squares the input and adds the anchor to prove it was received
    constexpr auto operator()(double x) const noexcept -> double { return (x * x) + config.cusp_anchor; }
};

// A mock quantizer to simulate the anchor nudging
template <typename projection_t> struct dummy_quantizer_t
{
    projection_t projection;

    // simulates unprojecting, snapping to a grid, and projecting back
    constexpr auto apply(double raw_anchor) const noexcept -> double
    {
        // For the PoC, we just pretend the quantizer nudges the value by +0.1
        // after evaluating the projection.
        static_cast<void>(projection);
        return raw_anchor + 0.1;
    }
};

//
// 3. Smoke Tests
//

struct shaping_architecture_test_t : testing::Test
{};

TEST_F(shaping_architecture_test_t, composition_evaluates_inside_out)
{
    // input_pipeline(x) = mul(add(x))
    auto const in_pipeline
        = composite_transform_t<dummy_in_mul_t, dummy_in_add_t>{dummy_in_mul_t{3.0}, dummy_in_add_t{2.0}};

    // (1.0 + 2.0) * 3.0 = 9.0
    EXPECT_DOUBLE_EQ(9.0, in_pipeline(1.0));
}

TEST_F(shaping_architecture_test_t, builder_flow_mutates_config_before_instantiation)
{
    // ------------------------------------------------------------------
    // STEP 1: Build the standalone input pipeline
    // ------------------------------------------------------------------
    auto const in_pipeline = composite_transform_t<dummy_in_mul_t, dummy_in_add_t>{
        dummy_in_mul_t{3.0}, // outer: e.g., offset
        dummy_in_add_t{2.0} // inner: e.g., affine
    };

    // ------------------------------------------------------------------
    // STEP 2: Quantize the curve configuration
    // ------------------------------------------------------------------
    auto raw_config = dummy_curve_config_t{.cusp_anchor = 5.0};

    auto const quantizer = dummy_quantizer_t<decltype(in_pipeline)>{in_pipeline};

    // Nudge the configuration *before* the curve exists
    raw_config.cusp_anchor = quantizer.apply(raw_config.cusp_anchor);
    EXPECT_DOUBLE_EQ(5.1, raw_config.cusp_anchor);

    // ------------------------------------------------------------------
    // STEP 3: Instantiate the curve with the nudged config
    // ------------------------------------------------------------------
    auto const curve = dummy_curve_t{raw_config};

    // ------------------------------------------------------------------
    // STEP 4: Build the standalone output pipeline
    // ------------------------------------------------------------------
    auto const out_pipeline = composite_transform_t<dummy_out_add_t, dummy_out_mul_t>{
        dummy_out_add_t{5.0}, // outer: e.g., limit
        dummy_out_mul_t{4.0} // inner: e.g., affine
    };

    // ------------------------------------------------------------------
    // STEP 5: Final Assembly (The Sandwich)
    // ------------------------------------------------------------------
    auto const composite = composite_curve_t<decltype(in_pipeline), dummy_curve_t, decltype(out_pipeline)>{
        in_pipeline, curve, out_pipeline};

    // ------------------------------------------------------------------
    // Trace the expected math:
    // ------------------------------------------------------------------
    // x = 1.0
    // 1. input:  (1.0 + 2.0) * 3.0 = 9.0
    // 2. curve:  (9.0 * 9.0) + 5.1 = 86.1
    // 3. output: (86.1 * 4.0) + 5.0 = 349.4

    EXPECT_DOUBLE_EQ(349.4, composite(1.0));
}

} // namespace
} // namespace crv::shaping
