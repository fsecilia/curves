// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "complex.hpp"
#include <crv/test/test.hpp>
#include <cmath>

namespace crv {
namespace {

using float_types_t = Types<float32_t, float64_t>;

template <typename real_t> constexpr real_t tolerance;
template <> constexpr auto tolerance<float32_t> = static_cast<float32_t>(1e-4);
template <> constexpr auto tolerance<float64_t> = static_cast<float64_t>(1e-10);

template <typename real_t> constexpr real_t step;
template <> constexpr auto step<float32_t> = static_cast<float32_t>(1e-10);
template <> constexpr auto step<float64_t> = static_cast<float64_t>(1e-50);

//
// fixture
//

struct complex_step_erf_test_t : Test
{
    template <typename real_t>
    auto test(std::complex<real_t> expected, std::complex<real_t> input) const noexcept -> void
    {
        auto const actual = complex_step_erf(input);

        constexpr auto const tolerance = crv::tolerance<real_t>;
        EXPECT_NEAR(expected.real(), actual.real(), tolerance);
        EXPECT_NEAR(expected.imag(), actual.imag(), tolerance);
    }
};

//
// asymptotes
//

template <typename real_t> struct complex_step_erf_test_asymptotes_t : complex_step_erf_test_t
{
    auto test_origin() const noexcept -> void
    {
        using complex_t = std::complex<real_t>;
        constexpr auto const step = crv::step<real_t>;

        test(complex_t{real_t{0}, step}, complex_t{});
    }

    auto test_far_field() const noexcept -> void
    {
        using complex_t = std::complex<real_t>;
        constexpr auto const step = crv::step<real_t>;

        test(complex_t{real_t{1}, real_t{0}}, complex_t{real_t{10}, step});
    }
};

TYPED_TEST_SUITE(complex_step_erf_test_asymptotes_t, float_types_t);

TYPED_TEST(complex_step_erf_test_asymptotes_t, origin)
{
    this->test_origin();
}

TYPED_TEST(complex_step_erf_test_asymptotes_t, far_field)
{
    this->test_far_field();
}

//
// composite derivative
//

struct complex_step_erf_test_composite_derivative_t : complex_step_erf_test_t, WithParamInterface<float64_t>
{
    float64_t const input_64 = GetParam();

    template <typename real_t> auto test() const noexcept -> void
    {
        using complex_t = std::complex<real_t>;

        using std::erf;
        using std::exp;

        auto const input = static_cast<real_t>(input_64);
        constexpr auto const step = crv::step<real_t>;
        constexpr auto two_over_sqrt_pi = real_t{2} * std::numbers::inv_sqrtpi_v<real_t>;

        // exact analytic derivative: f'(x) = erf(x) + x * (2/sqrt(pi)) * e^{-x^2}
        auto const expected = erf(input) + input * two_over_sqrt_pi * exp(-input * input);

        // composite function: f(x) = x * erf(x)
        static_assert(std::same_as<decltype(input), decltype(step)>);
        auto const z = complex_t{input, step};

        // extract complex-step derivative: Im(f(x + ih))/h
        auto const complex_result = z * complex_step_erf(z);
        auto const actual = complex_result.imag() / step;

        constexpr auto const tolerance = crv::tolerance<real_t>;
        EXPECT_NEAR(expected, actual, tolerance);
    }
};

TEST_P(complex_step_erf_test_composite_derivative_t, float32)
{
    test<float32_t>();
}

TEST_P(complex_step_erf_test_composite_derivative_t, float64)
{
    test<float64_t>();
}

float64_t const params[] = {-2.5, -1.0, -0.5, 0.5, 1.0, 2.5};
INSTANTIATE_TEST_SUITE_P(params, complex_step_erf_test_composite_derivative_t, ValuesIn(params));

} // namespace
} // namespace crv
