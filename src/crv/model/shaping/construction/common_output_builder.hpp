// SPDX-License-Identifier: MIT

/// \file
/// \brief constructs common output scale and positioning
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/config.hpp>
#include <crv/model/curves/concepts.hpp>
#include <crv/model/shaping/output_positioned_curve.hpp>
#include <crv/model/shaping/output_scaled_curve.hpp>
#include <crv/model/shaping/transforms/output_position.hpp>
#include <crv/model/shaping/transforms/output_scale.hpp>
#include <cassert>
#include <cmath>
#include <expected>
#include <utility>

namespace crv::shaping::construction {

enum class common_output_error_t : uint8_t
{
    output_scale_not_finite,
    positioning_height_not_finite,
    positioning_mode_invalid,
    fixed_anchor_negative,
    origin_outside_domain,
    domain_end_outside_domain,
    scaled_origin_not_representable,
    scaled_domain_end_not_representable,
    positioned_origin_negative,
};

/// builds common output scale followed by common positioning
struct common_output_builder_t
{
    using error_t = common_output_error_t;

    template <typename curve_t>
        requires is_curve<curve_t, typename curve_t::scalar_t>
    using scaled_curve_t = output_scaled_curve_t<transforms::output_scale_t<typename curve_t::scalar_t>, curve_t>;

    template <typename curve_t>
        requires is_curve<curve_t, typename curve_t::scalar_t>
    using positioned_curve_t
        = output_positioned_curve_t<transforms::output_position_t<typename curve_t::scalar_t>, scaled_curve_t<curve_t>>;

    template <typename curve_t>
        requires is_curve<curve_t, typename curve_t::scalar_t>
    using result_t = std::expected<positioned_curve_t<curve_t>, error_t>;

    template <typename curve_t>
        requires is_curve<curve_t, typename curve_t::scalar_t>
    [[nodiscard]] auto operator()(curve_t curve, model::common_curve_config_t const& config,
        typename curve_t::scalar_t domain_end) const -> result_t<curve_t>
    {
        using scalar_t = curve_t::scalar_t;
        using scale_t = transforms::output_scale_t<scalar_t>;
        using position_t = transforms::output_position_t<scalar_t>;

        assert(std::isfinite(domain_end) && domain_end >= scalar_t{0} && "common_output_builder_t: invalid domain end");

        auto const scale_value = static_cast<scalar_t>(config.scale.output.value());
        if (!std::isfinite(scale_value)) return std::unexpected{error_t::output_scale_not_finite};
        assert(scale_value > scalar_t{0} && "common_output_builder_t: authored output scale must be positive");
        auto scale = std::move(scale_t::make(scale_value)).value();

        auto const domain = curve.input_domain();
        if (!domain.contains(scalar_t{0})) return std::unexpected{error_t::origin_outside_domain};
        if (!domain.contains(domain_end)) return std::unexpected{error_t::domain_end_outside_domain};

        auto const scaled_origin = scale.try_apply(curve(scalar_t{0}));
        if (!scaled_origin) return std::unexpected{error_t::scaled_origin_not_representable};

        auto const scaled_domain_end = scale.try_apply(curve(domain_end));
        if (!scaled_domain_end) return std::unexpected{error_t::scaled_domain_end_not_representable};

        auto const mode = config.anchor.mode.value();
        auto const height = static_cast<scalar_t>(config.anchor.height.value());
        if (!std::isfinite(height)) return std::unexpected{error_t::positioning_height_not_finite};

        if (mode != model::anchor_mode_t::offset && mode != model::anchor_mode_t::fixed)
        {
            return std::unexpected{error_t::positioning_mode_invalid};
        }

        auto source_level = scalar_t{};
        switch (mode)
        {
            case model::anchor_mode_t::offset: break;
            case model::anchor_mode_t::fixed:
                if (height < scalar_t{0}) return std::unexpected{error_t::fixed_anchor_negative};
                source_level = *scaled_origin;
                break;
        }

        auto position_result = position_t::make(source_level, height);
        assert(position_result.has_value() && "common_output_builder_t: validated positioning levels must construct");
        auto position = std::move(*position_result);

        auto const positioned_origin = position.apply(*scaled_origin);
        if (positioned_origin < scalar_t{0}) return std::unexpected{error_t::positioned_origin_negative};
        assert(position.try_apply(*scaled_domain_end).has_value()
            && "common_output_builder_t: valid positioning must remain representable at domain end");

        auto scaled_curve = scaled_curve_t<curve_t>{std::move(scale), std::move(curve)};
        return positioned_curve_t<curve_t>{std::move(position), std::move(scaled_curve)};
    }
};

} // namespace crv::shaping::construction
