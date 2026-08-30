// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/model/shaping/transforms/compact_output_limiter.hpp>
#include <crv/model/shaping/transitions/construction/transition_factory_builder.hpp>
#include <crv/quadrature/antiderivative_factory.hpp>
#include <crv/test/test.hpp>
#include <cmath>

namespace crv::shaping::transforms {
namespace {

struct shaping_compact_output_limiter_transition_integration_test_t : Test,
                                                                      WithParamInterface<transitions::continuity_t>
{
    using scalar_t = float_t;
    using jet_t = crv::jet_t<scalar_t>;
    using antiderivative_factory_t = quadrature::antiderivative_factory_t<scalar_t>;
    using builder_t = transitions::construction::transition_factory_builder_t<antiderivative_factory_t>;
    using factory_t = builder_t::factory_t;

    static constexpr auto bound = scalar_t{2};
    static constexpr auto delta_y = scalar_t{0.5};
    static constexpr auto tolerance = scalar_t{2e-12};

    builder_t builder{antiderivative_factory_t{}, scalar_t{1e-12}, int_t{32}};
    factory_t factory = builder();
};

TEST_P(shaping_compact_output_limiter_transition_integration_test_t, upper_preserves_linear_delta_y_semantics)
{
    auto const actual = factory(GetParam(), []<typename product_t>(product_t product) -> scalar_t {
        using transition_t = typename product_t::transition_t;
        auto const sut
            = upper_output_limiter_t<scalar_t, transition_t>::make(bound, delta_y, product.transition).value();
        return sut(bound);
    });
    EXPECT_NEAR(actual, bound - delta_y, tolerance);
}

TEST_P(shaping_compact_output_limiter_transition_integration_test_t, lower_preserves_linear_delta_y_semantics)
{
    auto const actual = factory(GetParam(), []<typename product_t>(product_t product) -> scalar_t {
        using transition_t = typename product_t::transition_t;
        auto const sut
            = lower_output_limiter_t<scalar_t, transition_t>::make(bound, delta_y, product.transition).value();
        return sut(bound);
    });
    EXPECT_NEAR(actual, bound + delta_y, tolerance);
}

TEST_P(shaping_compact_output_limiter_transition_integration_test_t, upper_transition_never_exceeds_bound)
{
    auto const actual = factory(GetParam(), []<typename product_t>(product_t product) -> scalar_t {
        using transition_t = typename product_t::transition_t;
        auto const sut
            = upper_output_limiter_t<scalar_t, transition_t>::make(bound, delta_y, product.transition).value();
        return sut(bound);
    });
    EXPECT_LE(actual, bound);
}

TEST_P(shaping_compact_output_limiter_transition_integration_test_t, lower_transition_never_drops_below_bound)
{
    auto const actual = factory(GetParam(), []<typename product_t>(product_t product) -> scalar_t {
        using transition_t = typename product_t::transition_t;
        auto const sut
            = lower_output_limiter_t<scalar_t, transition_t>::make(bound, delta_y, product.transition).value();
        return sut(bound);
    });
    EXPECT_GE(actual, bound);
}

TEST_P(shaping_compact_output_limiter_transition_integration_test_t, upper_transition_derivative_is_nondecreasing_map)
{
    auto const actual = factory(GetParam(), []<typename product_t>(product_t product) -> scalar_t {
        using transition_t = typename product_t::transition_t;
        auto const sut
            = upper_output_limiter_t<scalar_t, transition_t>::make(bound, delta_y, product.transition).value();
        return sut(jet_t{bound, scalar_t{1}}).df;
    });
    EXPECT_GE(actual, scalar_t{0});
}

TEST_P(shaping_compact_output_limiter_transition_integration_test_t, lower_transition_derivative_is_nondecreasing_map)
{
    auto const actual = factory(GetParam(), []<typename product_t>(product_t product) -> scalar_t {
        using transition_t = typename product_t::transition_t;
        auto const sut
            = lower_output_limiter_t<scalar_t, transition_t>::make(bound, delta_y, product.transition).value();
        return sut(jet_t{bound, scalar_t{1}}).df;
    });
    EXPECT_GE(actual, scalar_t{0});
}

INSTANTIATE_TEST_SUITE_P(shaping_compact_output_limiter_real_transitions,
    shaping_compact_output_limiter_transition_integration_test_t,
    Values(transitions::continuity_t::c1, transitions::continuity_t::c2, transitions::continuity_t::c3,
        transitions::continuity_t::cinfinity));

struct nast_probe_curve_t
{
    float_t output;
    int_t* jet_calls;

    [[nodiscard]] auto operator()(float_t) const noexcept -> float_t { return output; }

    [[nodiscard]] auto operator()(jet_t<float_t> input) const noexcept -> jet_t<float_t>
    {
        ++*jet_calls;
        return {output, tangent(input)};
    }
};

TEST_F(shaping_compact_output_limiter_transition_integration_test_t,
    interior_nast_underflow_does_not_create_exact_plateau_control_flow)
{
    auto const actual = factory(
        transitions::continuity_t::cinfinity, []<typename product_t>(product_t product) -> std::pair<bool, int_t> {
            using transition_t = typename product_t::transition_t;
            auto const u = scalar_t{0.001};
            auto const half_integral = product.transition.antiderivative(scalar_t{0.5});
            auto const half_width = std::log1p(delta_y / bound) / (scalar_t{2} * half_integral);
            auto const lower_log = std::log(bound) - half_width;
            auto const curve_output = std::exp(lower_log + scalar_t{2} * half_width * u);
            auto jet_calls = int_t{0};
            auto const limiter
                = lower_output_limiter_t<scalar_t, transition_t>::make(bound, delta_y, product.transition).value();
            static_cast<void>(
                limiter.apply(nast_probe_curve_t{curve_output, &jet_calls}, jet_t{scalar_t{3}, scalar_t{1}}));
            return {product.transition.value(u) == scalar_t{0}, jet_calls};
        });
    EXPECT_EQ(actual, (std::pair{true, int_t{1}}));
}

} // namespace
} // namespace crv::shaping::transforms
