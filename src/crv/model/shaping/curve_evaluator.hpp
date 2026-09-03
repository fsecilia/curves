// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/domain.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <expected>
#include <utility>
#include <vector>

namespace crv::shaping {

/// classifies invalid scalar evaluation results
enum class curve_evaluation_error_t : uint8_t
{
    negative_finite_result,
    negative_infinity,
    nan,
};

/// provides trusted and checked scalar curve evaluation
template <typename t_evaluator_t>
    requires std::floating_point<typename t_evaluator_t::scalar_t>
class curve_evaluator_t
{
public:
    using evaluator_t = t_evaluator_t;
    using scalar_t = evaluator_t::scalar_t;
    using result_t = std::expected<scalar_t, curve_evaluation_error_t>;

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

private:
    evaluator_t evaluator_;
};

} // namespace crv::shaping
