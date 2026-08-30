// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "concepts.hpp"
#include <stdexcept>
#include <vector>

namespace crv {
namespace {

using real_t = float_t;

//
// is_curve
//

struct test_domain_t
{
    [[nodiscard]] constexpr auto contains(real_t) const noexcept -> bool { return true; }
};

struct valid_curve_t
{
    using scalar_t = real_t;
    using domain_t = test_domain_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

// misses noexcept
struct throwing_curve_t
{
    using scalar_t = real_t;
    using domain_t = test_domain_t;

    [[nodiscard, noreturn]] constexpr auto operator()(real_t) const -> real_t { throw std::logic_error{""}; }
    [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

// returns void instead of real_t
struct wrong_return_curve_t
{
    using scalar_t = real_t;
    using domain_t = test_domain_t;

    constexpr auto operator()(real_t) const noexcept -> void {}
    [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

// takes two arguments
struct wrong_arity_curve_t
{
    using scalar_t = real_t;
    using domain_t = test_domain_t;

    [[nodiscard]] constexpr auto operator()(real_t x, real_t) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct wrong_scalar_curve_t
{
    using scalar_t = int_t;
    using domain_t = test_domain_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct missing_domain_type_curve_t
{
    using scalar_t = real_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto domain() const noexcept -> test_domain_t { return {}; }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct missing_domain_curve_t
{
    using scalar_t = real_t;
    using domain_t = test_domain_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct missing_critical_points_curve_t
{
    using scalar_t = real_t;
    using domain_t = test_domain_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
};

struct alternate_domain_t
{
    [[nodiscard]] constexpr auto contains(real_t) const noexcept -> bool { return true; }
};

struct wrong_domain_return_curve_t
{
    using scalar_t = real_t;
    using domain_t = test_domain_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto domain() const noexcept -> alternate_domain_t { return {}; }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct wrong_domain_contains_t
{
    [[nodiscard]] constexpr auto contains(real_t) const noexcept -> int_t { return 1; }
};

struct wrong_domain_contains_curve_t
{
    using scalar_t = real_t;
    using domain_t = wrong_domain_contains_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct wrong_critical_points_curve_t
{
    using scalar_t = real_t;
    using domain_t = test_domain_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
    [[nodiscard]] auto critical_points() const -> std::vector<int_t> { return {}; }
};

static_assert(is_curve<valid_curve_t, real_t>);
static_assert(!is_curve<throwing_curve_t, real_t>);
static_assert(!is_curve<wrong_return_curve_t, real_t>);
static_assert(!is_curve<wrong_arity_curve_t, real_t>);
static_assert(!is_curve<wrong_scalar_curve_t, real_t>);
static_assert(!is_curve<missing_domain_type_curve_t, real_t>);
static_assert(!is_curve<missing_domain_curve_t, real_t>);
static_assert(!is_curve<missing_critical_points_curve_t, real_t>);
static_assert(!is_curve<wrong_domain_return_curve_t, real_t>);
static_assert(!is_curve<wrong_domain_contains_curve_t, real_t>);
static_assert(!is_curve<wrong_critical_points_curve_t, real_t>);

} // namespace
} // namespace crv
