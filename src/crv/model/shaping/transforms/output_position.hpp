// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <expected>
#include <optional>

namespace crv::shaping::transforms {

enum class output_position_error_t : uint8_t
{
    source_level_not_finite,
    target_level_not_finite,
};

/// maps one output level onto another by vertical translation
///
/// Evaluates P(y) = (y - source_level) + target_level. Source and target levels are kept separate to preserve a fixed
/// target exactly when y == source_level, even when their magnitudes differ substantially.
template <std::floating_point t_scalar_t> class output_position_t
{
public:
    using scalar_t = t_scalar_t;
    using jet_t = crv::jet_t<scalar_t>;
    using construction_result_t = std::expected<output_position_t, output_position_error_t>;

    [[nodiscard]] static auto make(scalar_t source_level, scalar_t target_level) -> construction_result_t
    {
        if (!std::isfinite(source_level)) { return std::unexpected{output_position_error_t::source_level_not_finite}; }
        if (!std::isfinite(target_level)) { return std::unexpected{output_position_error_t::target_level_not_finite}; }
        return output_position_t{source_level, target_level};
    }

    [[nodiscard]] auto try_apply(scalar_t output) const noexcept -> std::optional<scalar_t>
    {
        if (!std::isfinite(output)) return std::nullopt;

        auto const delta = output - source_level_;
        if (!std::isfinite(delta)) return std::nullopt;

        auto const positioned = delta + target_level_;
        if (!std::isfinite(positioned)) return std::nullopt;
        return positioned;
    }

    [[nodiscard]] auto apply(scalar_t output) const noexcept -> scalar_t
    {
        assert(std::isfinite(output) && "output_position_t: source output must be finite");
        auto const delta = output - source_level_;
        assert(std::isfinite(delta) && "output_position_t: translation delta must be representable");
        auto const positioned = delta + target_level_;
        assert(std::isfinite(positioned) && "output_position_t: positioned output must be representable");
        return positioned;
    }

    [[nodiscard]] auto apply(jet_t output) const noexcept -> jet_t
    {
        auto const output_primal = primal(output);
        assert(std::isfinite(output_primal) && "output_position_t: source jet primal must be finite");

        [[maybe_unused]] auto const delta = output_primal - source_level_;
        assert(std::isfinite(delta) && "output_position_t: translation delta must be representable");
        assert(
            std::isfinite(delta + target_level_) && "output_position_t: positioned jet primal must be representable");

        return (output - source_level_) + target_level_;
    }

private:
    constexpr output_position_t(scalar_t source_level, scalar_t target_level) noexcept
        : source_level_{source_level}, target_level_{target_level}
    {}

    scalar_t source_level_;
    scalar_t target_level_;
};

} // namespace crv::shaping::transforms
