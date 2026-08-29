// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "compact_output_limiter.hpp"
#include <crv/model/shaping/transitions/construction/nast_builder.hpp>
#include <crv/model/shaping/transitions/nast.hpp>
#include <crv/model/shaping/transitions/smootherstep.hpp>
#include <crv/model/shaping/transitions/smootheststep.hpp>
#include <crv/model/shaping/transitions/smoothstep.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <utility>

namespace crv::shaping::transforms {
namespace {

using transition_types_t
    = Types<transitions::smoothstep_t, transitions::smootherstep_t, transitions::smootheststep_t, transitions::nast_t>;

template <typename transition_t> struct transition_factory_t
{
    auto operator()() const -> transition_t { return {}; }
};

template <> struct transition_factory_t<transitions::nast_t>
{
    auto operator()() const -> transitions::nast_t
    {
        using builder_t = transitions::construction::nast_builder_t<>;
        auto builder = builder_t{builder_t::integrator_t{builder_t::requested_tolerance, builder_t::depth_limit}};
        auto result = builder();
        return std::move(result).value().transition;
    }
};

template <typename t_transition_t> struct shaping_transforms_compact_output_limiter_transition_test_t : Test
{
    using scalar_t = float_t;
    using transition_t = t_transition_t;
    using upper_t = upper_output_limiter_t<scalar_t, transition_t>;
    using lower_t = lower_output_limiter_t<scalar_t, transition_t>;
    using jet_t = crv::jet_t<scalar_t>;

    static constexpr auto bound = scalar_t{2};
    static constexpr auto delta_y = scalar_t{0.5};
    static constexpr auto tolerance = scalar_t{2e-12};

    auto transition() const -> transition_t { return transition_factory_t<transition_t>{}(); }
    auto make_upper() const -> upper_t { return upper_t::make(bound, delta_y, transition()).value(); }
    auto make_lower() const -> lower_t { return lower_t::make(bound, delta_y, transition()).value(); }
};

TYPED_TEST_SUITE(shaping_transforms_compact_output_limiter_transition_test_t, transition_types_t);

TYPED_TEST(shaping_transforms_compact_output_limiter_transition_test_t, upper_preserves_linear_delta_y_semantics)
{
    auto const sut = this->make_upper();
    EXPECT_NEAR(sut(this->bound), this->bound - this->delta_y, this->tolerance);
}

TYPED_TEST(shaping_transforms_compact_output_limiter_transition_test_t, lower_preserves_linear_delta_y_semantics)
{
    auto const sut = this->make_lower();
    EXPECT_NEAR(sut(this->bound), this->bound + this->delta_y, this->tolerance);
}

TYPED_TEST(shaping_transforms_compact_output_limiter_transition_test_t, upper_transition_never_exceeds_bound)
{
    auto const sut = this->make_upper();
    EXPECT_LE(sut(this->bound), this->bound);
}

TYPED_TEST(shaping_transforms_compact_output_limiter_transition_test_t, lower_transition_never_drops_below_bound)
{
    auto const sut = this->make_lower();
    EXPECT_GE(sut(this->bound), this->bound);
}

TYPED_TEST(
    shaping_transforms_compact_output_limiter_transition_test_t, upper_transition_derivative_is_nondecreasing_map)
{
    auto const sut = this->make_upper();
    EXPECT_GE(sut(typename TestFixture::jet_t{this->bound, 1.0}).df, 0.0);
}

TYPED_TEST(
    shaping_transforms_compact_output_limiter_transition_test_t, lower_transition_derivative_is_nondecreasing_map)
{
    auto const sut = this->make_lower();
    EXPECT_GE(sut(typename TestFixture::jet_t{this->bound, 1.0}).df, 0.0);
}

} // namespace
} // namespace crv::shaping::transforms
