// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/model/domain.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <expected>
#include <utility>
#include <vector>

namespace crv::shaping {

/// classifies invalid curve evaluation inputs and results
enum class curve_evaluation_error_t : uint8_t
{
    negative_finite_result,
    negative_infinity,
    nan,
    unexpected_positive_infinity, // transform arithmetic overflow without sanctioned overflow evidence
    positive_infinite_primal,
    nonfinite_derivative,
    nonfinite_input_tangent,
};

/// provides trusted and checked curve evaluation
template <typename t_evaluator_t>
    requires std::floating_point<typename t_evaluator_t::scalar_t>
class curve_evaluator_t
{
public:
    using evaluator_t = t_evaluator_t;
    using scalar_t = evaluator_t::scalar_t;
    using result_t = std::expected<scalar_t, curve_evaluation_error_t>;
    using jet_t = crv::jet_t<scalar_t>;
    using jet_result_t = std::expected<jet_t, curve_evaluation_error_t>;

    constexpr explicit curve_evaluator_t(evaluator_t evaluator) noexcept : evaluator_{std::move(evaluator)} {}

    /// evaluates a curve with known scalar validity
    [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t
    {
        auto const output = evaluator_(input);
        assert(std::isfinite(output));
        assert(output >= scalar_t{0});
        return output;
    }

    /// forwards nonscalar evaluation to the mathematical evaluator
    template <typename value_t> [[nodiscard]] auto operator()(value_t input) const noexcept -> value_t
    {
        return evaluator_(input);
    }

    /// forwards the evaluator input domain
    [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<scalar_t>
    {
        return evaluator_.input_domain();
    }

    /// forwards evaluator critical points
    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return evaluator_.critical_points(); }

    /// evaluates and classifies scalar validity during construction
    [[nodiscard]] auto try_evaluate(scalar_t input) const noexcept -> result_t
    {
        auto const output = evaluator_(input);

        if (std::isfinite(output))
        {
            if (output < scalar_t{0}) return std::unexpected{curve_evaluation_error_t::negative_finite_result};
            return output;
        }

        if (std::isnan(output)) return std::unexpected{curve_evaluation_error_t::nan};
        if (output < scalar_t{0}) return std::unexpected{curve_evaluation_error_t::negative_infinity};

        return output;
    }

    /// evaluates a first-order jet with finite tangent and a finite nonnegative primal
    ///
    /// The input coordinate must belong to the evaluator's input domain. Rejects a nonfinite input tangent before
    /// evaluation, then classifies the output primal before the output tangent. Scalar overflow evidence is not a
    /// valid differentiated result.
    [[nodiscard]] auto try_evaluate(jet_t input) const noexcept -> jet_result_t
    {
        if (!std::isfinite(tangent(input))) return std::unexpected{curve_evaluation_error_t::nonfinite_input_tangent};

        auto const output = evaluator_(input);
        auto const value = primal(output);
        if (std::isnan(value)) return std::unexpected{curve_evaluation_error_t::nan};
        if (!std::isfinite(value))
        {
            if (value < scalar_t{0}) return std::unexpected{curve_evaluation_error_t::negative_infinity};
            return std::unexpected{curve_evaluation_error_t::positive_infinite_primal};
        }
        if (value < scalar_t{0}) return std::unexpected{curve_evaluation_error_t::negative_finite_result};
        if (!std::isfinite(tangent(output))) return std::unexpected{curve_evaluation_error_t::nonfinite_derivative};
        return output;
    }

private:
    evaluator_t evaluator_;
};

} // namespace crv::shaping
