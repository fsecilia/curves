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
    [[nodiscard]] auto evaluate(scalar_t input) const noexcept -> scalar_t
    {
        auto const output = evaluator_.evaluate(input);
        assert(std::isfinite(output));
        assert(output >= scalar_t{0});
        return output;
    }

    /// evaluates and classifies scalar validity during construction
    [[nodiscard]] auto try_evaluate(scalar_t input) const noexcept -> result_t
    {
        auto const output = evaluator_.evaluate(input);

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
