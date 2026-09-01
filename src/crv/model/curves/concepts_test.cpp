// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "concepts.hpp"
#include <stdexcept>
#include <vector>

namespace crv {
namespace {

using real_t = float_t;

struct valid_curve_t
{
    using scalar_t = real_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<real_t>
    {
        return model::input_domain_t<real_t>::full();
    }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct throwing_curve_t
{
    using scalar_t = real_t;

    [[nodiscard, noreturn]] constexpr auto operator()(real_t) const -> real_t { throw std::logic_error{""}; }
    [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<real_t>
    {
        return model::input_domain_t<real_t>::full();
    }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct wrong_return_curve_t
{
    using scalar_t = real_t;

    constexpr auto operator()(real_t) const noexcept -> void {}
    [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<real_t>
    {
        return model::input_domain_t<real_t>::full();
    }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct wrong_arity_curve_t
{
    using scalar_t = real_t;

    [[nodiscard]] constexpr auto operator()(real_t x, real_t) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<real_t>
    {
        return model::input_domain_t<real_t>::full();
    }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct wrong_scalar_curve_t
{
    using scalar_t = int_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<real_t>
    {
        return model::input_domain_t<real_t>::full();
    }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct missing_input_domain_curve_t
{
    using scalar_t = real_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct wrong_input_domain_curve_t
{
    using scalar_t = real_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<float32_t>
    {
        return model::input_domain_t<float32_t>::full();
    }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct reference_input_domain_curve_t
{
    using scalar_t = real_t;

    model::input_domain_t<real_t> domain{model::input_domain_t<real_t>::full()};

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<real_t> const&
    {
        return domain;
    }
    [[nodiscard]] auto critical_points() const -> std::vector<real_t> { return {}; }
};

struct missing_critical_points_curve_t
{
    using scalar_t = real_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<real_t>
    {
        return model::input_domain_t<real_t>::full();
    }
};

struct wrong_critical_points_curve_t
{
    using scalar_t = real_t;

    [[nodiscard]] constexpr auto operator()(real_t x) const noexcept -> real_t { return x; }
    [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<real_t>
    {
        return model::input_domain_t<real_t>::full();
    }
    [[nodiscard]] auto critical_points() const -> std::vector<int_t> { return {}; }
};

static_assert(is_curve<valid_curve_t, real_t>);
static_assert(!is_curve<throwing_curve_t, real_t>);
static_assert(!is_curve<wrong_return_curve_t, real_t>);
static_assert(!is_curve<wrong_arity_curve_t, real_t>);
static_assert(!is_curve<wrong_scalar_curve_t, real_t>);
static_assert(!is_curve<missing_input_domain_curve_t, real_t>);
static_assert(!is_curve<wrong_input_domain_curve_t, real_t>);
static_assert(!is_curve<reference_input_domain_curve_t, real_t>);
static_assert(!is_curve<missing_critical_points_curve_t, real_t>);
static_assert(!is_curve<wrong_critical_points_curve_t, real_t>);

} // namespace
} // namespace crv
