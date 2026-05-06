// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "approximant.hpp"
#include <crv/math/abs.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

struct spline_approximant_test_t : Test
{
    using real_t = float_t;
    using jet_t = jet_t<real_t>;
    using x_t = fixed_t<int16_t, 2>;
    using y_t = fixed_t<int32_t, 5>;
    using normalized_t = fixed_t<uint64_t, 64>;
    struct coeffs_t
    {};

    struct mock_segment_t
    {
        virtual ~mock_segment_t() = default;

        MOCK_METHOD(normalized_t, x_to_t, (x_t), (const, noexcept));
        MOCK_METHOD(y_t, evaluate, (normalized_t), (const, noexcept));
        MOCK_METHOD(coeffs_t const&, coeffs, (), (const, noexcept));
        MOCK_METHOD(int_t, log2_width, (), (const, noexcept));
    };
    StrictMock<mock_segment_t> mock_segment;

    struct segment_t
    {
        using x_t = spline_approximant_test_t::x_t;

        mock_segment_t* mock = nullptr;

        auto x_to_t(x_t x) const noexcept -> normalized_t { return mock->x_to_t(x); }
        auto evaluate(normalized_t t) const noexcept -> y_t { return mock->evaluate(t); }
        auto coeffs() const noexcept -> coeffs_t const& { return mock->coeffs(); }
        auto log2_width() const noexcept -> int_t { return mock->log2_width(); }
    };

    struct mock_segment_derivative_t
    {
        virtual ~mock_segment_derivative_t() = default;

        MOCK_METHOD(real_t, dy_dx, (coeffs_t const&, normalized_t, int_t), (const, noexcept));
    };
    StrictMock<mock_segment_derivative_t> mock_segment_derivative;

    struct segment_derivative_t
    {
        mock_segment_derivative_t* mock = nullptr;

        auto dy_dx(coeffs_t const& coeffs, normalized_t t, int_t log2_width) const noexcept -> real_t
        {
            return mock->dy_dx(coeffs, t, log2_width);
        }
    };

    static constexpr auto x0 = x_t::literal(3);

    using sut_t = approximant_t<real_t, segment_t, segment_derivative_t>;
    sut_t sut{x0, segment_t{&mock_segment}, segment_derivative_t{&mock_segment_derivative}};
};

TEST_F(spline_approximant_test_t, call_operator)
{
    auto const x_real = float_t{7};
    auto const x_fixed = x_t{7};
    auto const t = normalized_t::literal(11);
    auto const y_real = float_t{13};
    auto const y_fixed = y_t{13};
    auto const coeffs = coeffs_t{};
    auto const dy_dx = real_t{19};
    auto const log2_width = 4;

    auto const expected = jet_t{y_real, dy_dx};

    EXPECT_CALL(mock_segment, x_to_t(x_fixed - x0)).WillOnce(Return(t));
    EXPECT_CALL(mock_segment, evaluate(t)).WillOnce(Return(y_fixed));
    EXPECT_CALL(mock_segment, coeffs()).WillOnce(ReturnRef(coeffs));
    EXPECT_CALL(mock_segment, log2_width()).WillOnce(Return(log2_width));
    EXPECT_CALL(mock_segment_derivative, dy_dx(Ref(coeffs), t, log2_width)).WillOnce(Return(dy_dx));
    auto const actual = sut(x_real);

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv::spline
