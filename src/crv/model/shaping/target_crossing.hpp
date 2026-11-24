// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <expected>
#include <utility>
#include <variant>

namespace crv::shaping {

/// selects the target relation
enum class crossing_comparison_t : uint8_t
{
    inclusive,
    strict,
};

/// identifies the first satisfying input with finite output
template <std::floating_point scalar_t> struct finite_crossing_t
{
    scalar_t input;

    constexpr auto operator==(finite_crossing_t const&) const noexcept -> bool = default;
};

/// indicates no satisfying input within the search interval
struct crossing_absent_t
{
    constexpr auto operator==(crossing_absent_t const&) const noexcept -> bool = default;
};

/// identifies the first satisfying input within the interval when overflow precedes any finite crossing
template <std::floating_point scalar_t> struct crossing_frontier_t
{
    scalar_t input;

    constexpr auto operator==(crossing_frontier_t const&) const noexcept -> bool = default;
};

template <std::floating_point scalar_t>
using crossing_result_t = std::variant<finite_crossing_t<scalar_t>, crossing_absent_t, crossing_frontier_t<scalar_t>>;

/// resolves a finite target crossing using checked representable-order search
template <typename first_true_search_t> struct target_crossing_t
{
    [[no_unique_address]] first_true_search_t search;

    /// finds the first input satisfying f(x) >= target or f(x) > target
    ///
    /// The caller supplies a nonempty, ordered, finite inclusive interval within the curve's admissible input domain,
    /// a finite nonnegative target, and a valid comparison. The curve provides try_evaluate(x), returning an expected
    /// with either a finite nonnegative output, positive infinity, or an evaluation error. Checked evaluation must be
    /// deterministic. Successful outputs must be nondecreasing on the finite prefix, with only positive infinity after
    /// that prefix. Either region may be empty. Both zero encodings denote the same input. Errors from evaluated probes
    /// propagate; unprobed inputs are not validated. Positive infinity satisfies either relation but is classified as a
    /// frontier, not a finite crossing.
    template <std::floating_point scalar_t, typename curve_t>
        requires std::same_as<typename decltype(std::declval<curve_t const&>().try_evaluate(scalar_t{}))::value_type,
            scalar_t>
    [[nodiscard]] constexpr auto operator()(scalar_t low, scalar_t high, scalar_t target,
        crossing_comparison_t comparison, curve_t const& curve) const noexcept
    {
        using evaluation_result_t = decltype(curve.try_evaluate(low));
        using error_t = evaluation_result_t::error_type;
        using predicate_result_t = std::expected<bool, error_t>;
        using result_t = std::expected<crossing_result_t<scalar_t>, error_t>;

        assert(std::isfinite(target) && target >= scalar_t{0} && "target_crossing_t: invalid target");
        assert((comparison == crossing_comparison_t::inclusive || comparison == crossing_comparison_t::strict)
            && "target_crossing_t: invalid comparison");

        auto crossing = search(low, high, [&](scalar_t input) noexcept -> predicate_result_t {
            auto output = curve.try_evaluate(input);
            if (!output) return std::unexpected{std::move(output).error()};
            return comparison == crossing_comparison_t::inclusive ? *output >= target : *output > target;
        });
        if (!crossing) return result_t{std::unexpected{std::move(crossing).error()}};
        if (!*crossing) return result_t{crossing_absent_t{}};

        auto output = curve.try_evaluate(**crossing);
        if (!output) return result_t{std::unexpected{std::move(output).error()}};
        if (std::isfinite(*output)) return result_t{finite_crossing_t<scalar_t>{**crossing}};
        return result_t{crossing_frontier_t<scalar_t>{**crossing}};
    }
};

} // namespace crv::shaping
