// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "output_affine.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

namespace crv::signal_chain::stages {
namespace {

using real_t = float_t;

//
// stage doubles
//

struct identity_t
{
    template <typename value_t> constexpr auto forward(value_t x) const noexcept -> value_t { return x; }
    template <typename value_t> constexpr auto inverse(value_t y) const noexcept -> value_t { return y; }
};

struct add_five_t
{
    template <typename value_t> constexpr auto forward(value_t x) const noexcept -> value_t { return x + 5.0; }
    template <typename value_t> constexpr auto inverse(value_t y) const noexcept -> value_t { return y - 5.0; }
};

//
// single-stage tests
//

// single affine over terminator
constexpr auto a = output_affine_t{2.0, 3.0, identity_t{}};

// ground truth
static_assert(a.forward(10.0) == 23.0); // 2*10 + 3
static_assert(a.forward(-3.0 / 2.0) == 0.0); // output == shift maps to 0
static_assert(a.inverse(23.0) == 10.0); // (23 - 3)/2

// round trips
static_assert(a.inverse(a.forward(10.0)) == 10.0);
static_assert(a.forward(a.inverse(23.0)) == 23.0);

// sign handling on shift
constexpr auto const negative_shift = output_affine_t{2.0, -3.0, identity_t{}};
static_assert(negative_shift.forward(5.0) == 7.0); // 2*5 + -3)
static_assert(negative_shift.inverse(7.0) == 5.0);

// alternative scale
constexpr auto half = output_affine_t{0.5, 4.0, identity_t{}};
static_assert(half.forward(10.0) == 9.0); // 0.5*10 + 4
static_assert(half.inverse(9.0) == 10.0);

//
// multi-stage tests
//

// chain through asymmetric prev
constexpr auto b = output_affine_t{4.0, 7.0, add_five_t{}};

// ground truth pins order (affine then prev)
static_assert(b.forward(10.0) == 67.0); // 4*(10 + 5) + 7
static_assert(b.inverse(67.0) == 10.0); // (67 - 7)/4 - 5

// round trips still work
static_assert(b.inverse(b.forward(10.0)) == 10.0);
static_assert(b.forward(b.inverse(67.0)) == 67.0);

//
// composed-stage tests
//

// ab(x) == b(a(x))
constexpr auto ab = output_affine_t{2.0, 3.0, output_affine_t{4.0, 7.0, add_five_t{}}};
static_assert(ab.forward(10.0) == a.forward(b.forward(10.0)));
static_assert(ab.inverse(10.0) == b.inverse(a.inverse(10.0)));

//
// jet tests
//

static_assert(jet_t{23.0, 6.0} == a.forward(jet_t{10.0, 3.0}));
static_assert(jet_t{137.0, 24.0} == ab.forward(jet_t{10.0, 3.0}));

//
// runtime tests
//

struct output_affine_test_t : Test
{
    auto sweep(auto const& sut) const noexcept -> void
    {
        for (auto x = -50.0; x <= 50.0; x += 0.25)
        {
            using std::abs;
            EXPECT_NEAR(sut.inverse(sut.forward(x)), x, 1e-9 * (1.0 + abs(x)));
            EXPECT_NEAR(sut.forward(sut.inverse(x)), x, 1e-9 * (1.0 + abs(x)));
        }
    }
};

// messy round-trip sweeps
//
// A single round trip is insufficient. Sweep with messy floats that require tolerances to check
TEST_F(output_affine_test_t, symmetric_round_trip_sweeps)
{
    sweep(output_affine_t{1.3, -0.7, identity_t{}});
}

TEST_F(output_affine_test_t, asymmetric_round_trip_sweeps)
{
    sweep(output_affine_t{1.3, -0.7, add_five_t{}});
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(output_affine_test_t, invariant_death_test)
{
    EXPECT_DEBUG_DEATH((output_affine_t{-1.0, 0.0, identity_t{}}), "scale");
}

#endif

} // namespace
} // namespace crv::signal_chain::stages
