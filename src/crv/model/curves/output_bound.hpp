// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/inverse.hpp>
#include <crv/model/curves/concepts.hpp>
#include <crv/model/domain.hpp>
#include <crv/model/shaping/curve_evaluator.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <expected>
#include <optional>
#include <utility>
#include <variant>

namespace crv::model::curves {

/// classifies curve output bound resolution failures
enum class curve_output_bound_error_t : uint8_t
{
    frontier_preceded_bound,
};

using curve_output_bound_errors_t = std::variant<shaping::curve_evaluation_error_t, curve_output_bound_error_t>;

template <typename scalar_t>
using curve_output_bound_result_t = std::expected<std::optional<scalar_t>, curve_output_bound_errors_t>;

/// tests whether output reaches a lower bound
struct curve_output_lower_bound_relation_t
{
    template <typename scalar_t>
    [[nodiscard]] constexpr auto operator()(scalar_t output, scalar_t target) const noexcept -> bool
    {
        return output >= target;
    }
};

/// tests whether output exceeds an upper bound
struct curve_output_upper_bound_relation_t
{
    template <typename scalar_t>
    [[nodiscard]] constexpr auto operator()(scalar_t output, scalar_t target) const noexcept -> bool
    {
        return output > target;
    }
};

/// finds an output bound over a supplied interval of a monotone curve
template <typename first_true_search_t> struct curve_output_bound_search_t
{
    [[no_unique_address]] first_true_search_t find_first_true;

    template <typename curve_t, typename relation_t>
        requires is_curve<curve_t, typename curve_t::scalar_t>
        && requires(curve_t const& curve, typename curve_t::scalar_t input) {
               {
                   curve.try_evaluate(input)
               } -> std::same_as<std::expected<typename curve_t::scalar_t, shaping::curve_evaluation_error_t>>;
           }
    [[nodiscard]] auto operator()(curve_t const& curve, input_domain_t<typename curve_t::scalar_t> search_domain,
        typename curve_t::scalar_t target, relation_t const& relation) const noexcept
        -> curve_output_bound_result_t<typename curve_t::scalar_t>
    {
        using scalar_t = curve_t::scalar_t;
        using predicate_result_t = std::expected<bool, shaping::curve_evaluation_error_t>;

        assert(std::isfinite(target) && "curve output bound target must be finite");
        if (search_domain.empty()) return std::optional<scalar_t>{};

        auto const curve_domain = curve.input_domain();
        assert(curve_domain.contains(search_domain.first()) && curve_domain.contains(search_domain.last())
            && "curve output bound search interval outside curve input domain");

        auto const predicate = [&curve, &relation, target](scalar_t input) noexcept -> predicate_result_t {
            auto const output = curve.try_evaluate(input);
            if (!output) return std::unexpected{output.error()};
            return relation(*output, target);
        };

        auto const first_true = find_first_true(search_domain.first(), search_domain.last(), predicate);
        if (!first_true)
        {
            return std::unexpected{
                curve_output_bound_errors_t{std::in_place_type<shaping::curve_evaluation_error_t>, first_true.error()}};
        }
        if (!*first_true) return std::optional<scalar_t>{};

        auto const input = **first_true;
        auto const output = curve.try_evaluate(input);
        if (!output)
        {
            return std::unexpected{
                curve_output_bound_errors_t{std::in_place_type<shaping::curve_evaluation_error_t>, output.error()}};
        }

        if (!std::isfinite(*output))
        {
            assert(std::isinf(*output) && *output > scalar_t{0} && "checked curve returned invalid successful result");
            return std::unexpected{curve_output_bound_errors_t{
                std::in_place_type<curve_output_bound_error_t>, curve_output_bound_error_t::frontier_preceded_bound}};
        }

        assert(relation(*output, target) && "curve output bound search returned point that does not satisfy relation");
        return std::optional<scalar_t>{input};
    }
};

/// binds an output search to its relation
template <typename search_t, typename relation_t> struct curve_output_bound_t
{
    using error_t = curve_output_bound_errors_t;

    template <typename scalar_t> using result_t = curve_output_bound_result_t<scalar_t>;

    [[no_unique_address]] search_t search;
    [[no_unique_address]] relation_t relation;

    template <typename curve_t>
    [[nodiscard]] auto operator()(curve_t const& curve, input_domain_t<typename curve_t::scalar_t> search_domain,
        typename curve_t::scalar_t target) const noexcept -> result_t<typename curve_t::scalar_t>
    {
        return search(curve, search_domain, target, relation);
    }
};

/// finds the first input where a monotone curve reaches an output bound
using curve_output_lower_bound_t
    = curve_output_bound_t<curve_output_bound_search_t<try_bisect_first_true_t>, curve_output_lower_bound_relation_t>;

/// finds the first input where a monotone curve exceeds an output bound
using curve_output_upper_bound_t
    = curve_output_bound_t<curve_output_bound_search_t<try_bisect_first_true_t>, curve_output_upper_bound_relation_t>;

} // namespace crv::model::curves
