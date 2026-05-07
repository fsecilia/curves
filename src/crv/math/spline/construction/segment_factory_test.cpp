// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_factory.hpp"
#include <crv/math/integer.hpp>
#include <crv/test/test.hpp>
#include <limits>

namespace crv::spline {
namespace {

using real_t = float_t;
using jet_t = jet_t<real_t>;

// nan-aware jet comparison
constexpr auto jet_eq(jet_t lhs, jet_t rhs) noexcept -> bool
{
    if (lhs == rhs) return true;

    // std::isnan should be constexpr safe, but we know !(x == x) is constexpr-safe, so use that instead
    if (lhs.f == rhs.f && !(lhs.df == lhs.df) && !(rhs.df == rhs.df)) return true;
    if (!(lhs.f == lhs.f) && !(rhs.f == rhs.f) && lhs.df == rhs.df) return true;
    return !(lhs.f == lhs.f) && !(rhs.f == rhs.f) && !(lhs.df == lhs.df) && !(rhs.df == rhs.df);
}

struct hermite_converter_t
{
    struct result_t
    {
        jet_t left;
        jet_t right;

        constexpr auto operator==(result_t const& src) const noexcept -> bool
        {
            return jet_eq(left, src.left) && jet_eq(right, src.right);
        }
    };

    constexpr auto operator()(jet_t left, jet_t right) const noexcept -> result_t { return {left, right}; }
};

struct segment_t
{
    struct packed_segment_t
    {
        hermite_converter_t::result_t hermite_converter_result;
        int_t log2_width;
        constexpr auto operator==(packed_segment_t const& src) const noexcept -> bool = default;
    };
    packed_segment_t packed_segment;

    constexpr auto operator==(segment_t const& src) const noexcept -> bool = default;
};

constexpr auto signed_shift(int_t log2_width) noexcept -> real_t
{
    if (log2_width < 0) return 1.0 / (1 << -log2_width);
    else return static_cast<real_t>(1 << log2_width);
}

struct segment_derivative_t
{
    constexpr auto dx_dt(int_t log2_width) const noexcept -> real_t { return signed_shift(log2_width); }
};

using sut_t = segment_factory_t<real_t, segment_t, segment_derivative_t, hermite_converter_t>;
constexpr sut_t sut{};

constexpr auto inf = std::numeric_limits<real_t>::infinity();
constexpr auto nan = std::numeric_limits<real_t>::quiet_NaN();
constexpr auto max_val = std::numeric_limits<real_t>::max();
constexpr auto lowest_val = std::numeric_limits<real_t>::lowest();
constexpr auto eps = std::numeric_limits<real_t>::epsilon();

constexpr auto test(jet_t left, jet_t right, int_t log2_width) noexcept -> bool
{
    using packed_segment_t = segment_t::packed_segment_t;
    auto width = signed_shift(log2_width);
    return sut(left, right, log2_width)
        == segment_t{packed_segment_t{{{left.f, left.df * width}, {right.f, right.df * width}}, log2_width}};
}

// degenerate case
static_assert(test({0.0, 0.0}, {0.0, 0.0}, 0));

// identity scaling: log2_width = 0 means width = 1, df passes through unchanged
static_assert(test({3.0, 5.0}, {7.0, 11.0}, 0));

//
// typical cases
//

// all positive
static_assert(test({3.0, 5.0}, {7.0, 11.0}, 3));

// all negative
static_assert(test({-3.0, -5.0}, {-7.0, -11.0}, -2));

// mixed signs
static_assert(test({1.0, -2.0}, {-3.0, 4.0}, 4));
static_assert(test({-1.0, 2.0}, {3.0, -4.0}, -4));

// unit tangent: df = 1.0 confirms scaling produces exactly width, not some off-by-one transform
static_assert(test({2.0, 1.0}, {8.0, 1.0}, 3));

//
// structural cases
//

// flat tangents
static_assert(test({5.0, 0.0}, {7.0, 0.0}, 2));

// zero-crossings
static_assert(test({0.0, 5.0}, {0.0, 7.0}, -1));

// asymmetric activity
static_assert(test({0.0, 0.0}, {10.0, -10.0}, 3));
static_assert(test({10.0, -10.0}, {0.0, 0.0}, 3));

// local extremum: equal f with opposing tangents
static_assert(test({5.0, 2.0}, {5.0, -3.0}, 2));

//
// boundary cases
//

// max and lowest finite values
TEST(spline_segment_factory_test, max_and_lowest_finite_values)
{
    // gcc does not compile this constexpr, so this must be runtime
    EXPECT_TRUE(test({max_val, max_val}, {lowest_val, lowest_val}, 1));
}

// subnormals/epsilon
static_assert(test({eps, eps}, {-eps, -eps}, -2));

// infinity
static_assert(test({inf, inf}, {inf, inf}, 1));
static_assert(test({-inf, -inf}, {-inf, -inf}, 1));

// mixed finite/infinite within a single jet: f and df are independent channels
static_assert(test({1.0, inf}, {inf, 1.0}, 2));

// nan propagation: nan in f passes through, nan in df stays nan after scaling
TEST(spline_segment_factory_test, nan_propagation)
{
    // clang does not accept constexpr nan, so these must be runtime
    EXPECT_TRUE(test({nan, 1.0}, {2.0, 3.0}, 1));
    EXPECT_TRUE(test({1.0, nan}, {2.0, 3.0}, 1));
    EXPECT_TRUE(test({nan, nan}, {nan, nan}, 1));
}

} // namespace
} // namespace crv::spline
