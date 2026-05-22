// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "critical_point_conditioner.hpp"
#include <crv/test/test.hpp>

namespace crv::spline::generic {
namespace {

using x_t = fixed_t<int_t, 8>;
constexpr auto log2_min_width = -2;
constexpr auto min_width = x_t{1} >> -log2_min_width;
constexpr auto epsilon = x_t::literal(1);

constexpr auto z = x_t{0};
constexpr auto d = min_width;
constexpr auto e = epsilon;

using sut_t = critical_point_conditioner_t<x_t, log2_min_width>;
using points_t = sut_t::points_t;

constexpr auto sut = sut_t{};

//
// individual points
//

// points are quantied to nearest
static_assert(sut(z) == z);
static_assert(sut(e) == z);
static_assert(sut(d / 2 - epsilon) == z);
static_assert(sut(d / 2) == d);
static_assert(sut(d * 3 / 2 - epsilon) == d);
static_assert(sut(d * 3 / 2) == d * 2);

// negative points are quantized to nearest (ties break towards +inf)
static_assert(sut(-e) == z);
static_assert(sut(-d / 2) == z); // tie breaks to 0
static_assert(sut(-d / 2 - e) == -d);
static_assert(sut(-d) == -d);
static_assert(sut(-d * 3 / 2) == -d); // tie breaks to -d
static_assert(sut(-d * 3 / 2 - e) == -d * 2);

// values near limits quantize cleanly without ub
static_assert(sut(points_t{max<x_t>()})[0] == sut(max<x_t>()));
static_assert(sut(points_t{min<x_t>()})[0] == sut(min<x_t>()));

//
// arrays
//

// empty arrays remain empty
static_assert(sut(points_t{}) == points_t{});

// arrays are quantized
static_assert(sut({d, 2 * d + e, 3 * d - e, 9 * d / 2 - e, 9 * d / 2}) == points_t{d, 2 * d, 3 * d, 4 * d, 5 * d});

// arrays are sorted ascending
static_assert(sut(points_t{d * 5, d * 2, d, d * 7}) == points_t{d, d * 2, d * 5, d * 7});

// duplicates are removed
static_assert(
    sut(points_t{d, 2 * d, 3 * d, 3 * d, 4 * d, 4 * d, 4 * d, 5 * d}) == points_t{d, 2 * d, 3 * d, 4 * d, 5 * d});

// duplicates are removed after sorting
static_assert(
    sut(points_t{d, 3 * d, 2 * d, 4 * d, 3 * d, 4 * d, 5 * d, 4 * d}) == points_t{d, 2 * d, 3 * d, 4 * d, 5 * d});

// duplicates are removed after quantizing
static_assert(sut(points_t{d, 2 * d + e, 2 * d, 3 * d, 3 * d - e, 9 * d / 2 - e, 4 * d, 5 * d, 9 * d / 2})
    == points_t{d, 2 * d, 3 * d, 4 * d, 5 * d});

// duplicates are removed after quantizing and sorting
static_assert(sut(points_t{3 * d, 2 * d + e, d, 2 * d, 9 * d / 2 - e, 5 * d, 3 * d - e, 9 * d / 2, 4 * d})
    == points_t{d, 2 * d, 3 * d, 4 * d, 5 * d});

// negative arrays are sorted ascending
static_assert(sut(points_t{-d * 5, -d * 2, -d, -d * 7}) == points_t{-d * 7, -d * 5, -d * 2, -d});

// arrays crossing zero are quantized, sorted, and uniqued
static_assert(sut(points_t{d, -d, z, d * 2, -d * 2, -d / 2, d / 2}) == points_t{-d * 2, -d, z, d, d * 2});

// many distinct values all quantize into the same value
static_assert(sut(points_t{e, z, -e, d / 2 - e, -d / 2, d / 4, -d / 4}) == points_t{z});

} // namespace
} // namespace crv::spline::generic
