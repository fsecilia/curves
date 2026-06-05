// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/complex_traits.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/signal_chain/curves/traits.hpp>
#include <crv/signal_chain/quadrature/adaptive_integrator.hpp>
#include <crv/test/test.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <expected>
#include <iostream>
#include <limits>
#include <numbers>
#include <numeric>
#include <optional>
#include <utility>

namespace crv::signal_chain {

template <std::floating_point real_t> inline constexpr real_t min_spline_transition_width = real_t{1e-2};

// --------------------------------------------------------------------------------------------------------------------h

enum class warp_error_t
{
    invalid_limit_width, // limit_width <= 0
    invalid_offset_width, // offset_width <= 0 when offset_center is provided
    transition_overlap, // onset saturates after the limit band begins
    limit_placement_failure // limit inversion geometry drifted beyond tolerance
};

constexpr auto to_string(warp_error_t error) noexcept -> std::string_view
{
    switch (error)
    {
        case warp_error_t::invalid_limit_width: return "Limit width too small.";
        case warp_error_t::invalid_offset_width: return "Offset width too small.";
        case warp_error_t::transition_overlap: return "Offset and limit transitions overlap.";
        case warp_error_t::limit_placement_failure: return "Precision loss: limit cap deviates from the curve.";
    }
    return "Unknown warp error.";
}

// =============================================================================
// erf saturation threshold and width->sharpness mapping
// =============================================================================

template <std::floating_point real_t>
inline real_t const erf_saturation_z = [] {
    using std::log;
    using std::sqrt;
    return sqrt(-log(std::numeric_limits<real_t>::epsilon()));
}();

template <std::floating_point real_t> [[nodiscard]] constexpr auto erf_sharpness_from_width(real_t w) noexcept -> real_t
{
    constexpr real_t cube_root_30_sqrt_pi = static_cast<real_t>(3.7603828531416483L);
    return cube_root_30_sqrt_pi / w;
}

// =============================================================================
// one half of the warp velocity profile
// =============================================================================

template <std::floating_point t_real_t> class erf_half_t
{
public:
    using real_t = t_real_t;

    erf_half_t(real_t center, real_t k) noexcept
        : center_{center}, k_{k}, half_width_{erf_saturation_z<real_t> / std::abs(k)}
    {}

    [[nodiscard]] constexpr auto center() const noexcept -> real_t { return center_; }
    [[nodiscard]] constexpr auto k() const noexcept -> real_t { return k_; }

    [[nodiscard]] constexpr auto saturation_half_width() const noexcept -> real_t { return half_width_; }
    [[nodiscard]] constexpr auto band_lo() const noexcept -> real_t { return center_ - half_width_; }
    [[nodiscard]] constexpr auto band_hi() const noexcept -> real_t { return center_ + half_width_; }

    /// value of phi' = 1/2 (1 + erf(k (x - c))), in the x-frame
    template <typename value_t> constexpr auto evaluate(value_t x) const noexcept -> value_t
    {
        using std::real;

        auto const sat = erf_saturation_z<real_t>;
        auto const z = static_cast<value_t>(k_) * (x - static_cast<value_t>(center_));

        if (real(z) <= -sat) return value_t{0};
        if (real(z) >= sat) return value_t{1};

        return value_t{0.5} * (value_t{1} + erf_(z));
    }

    template <typename value_t> constexpr auto integral(value_t x) const noexcept -> value_t
    {
        using std::exp;

        auto const z = static_cast<value_t>(k_) * (x - static_cast<value_t>(center_));
        auto const k = static_cast<value_t>(k_);
        auto const inv_sqrt_pi = static_cast<value_t>(inv_sqrt_pi_);

        auto const G = value_t{0.5} * (z * (value_t{1} + erf_(z)) + inv_sqrt_pi * exp(-(z * z)));
        return G / k;
    }

private:
    // This is *not* complex erf. It is erf with a complex approxiation within epsilon of the real line.
    template <typename value_t> static auto erf_(value_t z) -> value_t
    {
        using std::exp;
        if constexpr (is_complex<value_t>)
        {
            using real_type = real_type_t<value_t>;
            auto const a = z.real();
            auto const b = z.imag();
            using std::erf;
            return value_t{erf(a), b * static_cast<real_type>(two_over_sqrt_pi_) * exp(-a * a)};
        }
        else
        {
            using std::erf;
            return erf(z);
        }
    }

    static constexpr real_t inv_sqrt_pi_ = std::numbers::inv_sqrtpi_v<real_t>;
    static constexpr real_t two_over_sqrt_pi_ = real_t{2} * std::numbers::inv_sqrtpi_v<real_t>;

    real_t center_;
    real_t k_;
    real_t half_width_;
};

// =============================================================================
// affine input/output stages
// =============================================================================

template <typename real_t, typename prev_t> struct input_scale_t
{
    real_t scale;
    prev_t prev;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return prev(x * scale);
    }
};

template <typename real_t, typename prev_t> struct input_offset_t
{
    real_t offset;
    prev_t prev;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return prev(x - offset);
    }
};

template <typename real_t, typename prev_t> struct output_scale_t
{
    real_t scale;
    prev_t prev;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return prev(x) * scale;
    }
};

template <typename real_t, typename prev_t> struct output_offset_t
{
    real_t offset;
    prev_t prev;

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        return prev(x) + offset;
    }
};

template <typename real_t, typename prev_t> constexpr auto make_input_scale(real_t scale, prev_t prev)
{
    return input_scale_t<real_t, std::remove_cvref_t<prev_t>>{scale, std::move(prev)};
}
template <typename real_t, typename prev_t> constexpr auto make_input_offset(real_t offset, prev_t prev)
{
    return input_offset_t<real_t, std::remove_cvref_t<prev_t>>{offset, std::move(prev)};
}
template <typename real_t, typename prev_t> constexpr auto make_output_scale(real_t scale, prev_t prev)
{
    return output_scale_t<real_t, std::remove_cvref_t<prev_t>>{scale, std::move(prev)};
}
template <typename real_t, typename prev_t> constexpr auto make_output_offset(real_t offset, prev_t prev)
{
    return output_offset_t<real_t, std::remove_cvref_t<prev_t>>{offset, std::move(prev)};
}

// =============================================================================
// domain warp stage
// =============================================================================

template <std::floating_point t_real_t, typename t_prev_t> class domain_warp_t
{
public:
    using real_t = t_real_t;
    using prev_t = t_prev_t;
    using half_t = erf_half_t<real_t>;

    struct config_t
    {
        std::optional<half_t> offset;
        std::optional<half_t> limit;
        real_t lag = real_t{0};
        real_t phi_at_b2 = real_t{0};
        real_t phi_capped = real_t{0};
    };

    domain_warp_t(config_t config, prev_t prev) noexcept : config_{std::move(config)}, prev_{std::move(prev)} {}

    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t
    {
        if (!config_.offset.has_value() && !config_.limit.has_value()) return prev_(x);

        return prev_(warp_evaluate(x));
    }

private:
    enum class region_t
    {
        paused,
        onset,
        plateau,
        limit,
        capped
    };

    template <typename value_t> constexpr auto classify(value_t x) const noexcept -> region_t
    {
        using std::real;

        auto const rx = real(x);

        if (config_.offset.has_value())
        {
            if (rx <= config_.offset->band_lo()) return region_t::paused;
            if (rx < config_.offset->band_hi()) return region_t::onset;
        }

        if (config_.limit.has_value())
        {
            if (rx < config_.limit->band_lo()) return region_t::plateau;
            if (rx < config_.limit->band_hi()) return region_t::limit;
            return region_t::capped;
        }

        return region_t::plateau;
    }

    template <typename value_t> constexpr auto warp_evaluate(value_t x) const noexcept -> value_t
    {
        switch (classify(primal(x)))
        {
            case region_t::paused: return value_t{};
            case region_t::plateau: return x - config_.lag;
            case region_t::capped: return value_t{config_.phi_capped};
            case region_t::onset: return onset_evaluate(x);
            case region_t::limit: return limit_evaluate(x);
        }
        return value_t{};
    }

    template <typename value_t> constexpr auto onset_evaluate(value_t x) const noexcept -> value_t
    {
        auto const& half = *config_.offset;

        auto const p = primal(x);
        auto const phi = half.integral(p) - half.integral(half.band_lo());

        if constexpr (is_jet<value_t>) return value_t{phi, half.evaluate(p) * tangent(x)};
        else return phi;
    }

    template <typename value_t> constexpr auto limit_evaluate(value_t x) const noexcept -> value_t
    {
        auto const& half = *config_.limit;

        auto const p = primal(x);
        auto const phi = config_.phi_at_b2 + (half.integral(p) - half.integral(half.band_lo()));

        if constexpr (is_jet<value_t>) return value_t{phi, half.evaluate(p) * tangent(x)};
        else return phi;
    }

    config_t config_;
    prev_t prev_;
};

// =============================================================================
// domain warp factory
// =============================================================================

namespace detail {

template <std::floating_point real_t, typename g_t>
[[nodiscard]] constexpr auto bisect_crossing(real_t lo, real_t hi, real_t target, g_t const& g) noexcept
    -> std::optional<real_t>
{
    if (g(hi) < target) return std::nullopt;
    if (g(lo) >= target) return lo;

    while (true)
    {
        auto const mid = std::midpoint(lo, hi);
        if (mid == lo || mid == hi) break;
        if (g(mid) < target) lo = mid;
        else hi = mid;
    }
    return hi;
}

} // namespace detail

template <std::floating_point real_t, real_t min_spline_transition_width> struct domain_warp_factory_t
{
    template <typename prev_t>
    [[nodiscard]] auto operator()(prev_t prev, real_t limit, real_t limit_width, std::optional<real_t> offset_center,
        real_t offset_width, real_t domain_lo, real_t domain_hi)
        -> std::expected<domain_warp_t<real_t, prev_t>, warp_error_t>
    {
        using warp_t = domain_warp_t<real_t, prev_t>;
        using half_t = erf_half_t<real_t>;
        using config_t = typename warp_t::config_t;

        if (limit_width <= min_spline_transition_width) return std::unexpected(warp_error_t::invalid_limit_width);

        // --- offset placement (optional) ---
        auto offset_half = std::optional<half_t>{};
        auto lag = real_t{0};
        if (offset_center.has_value())
        {
            if (offset_width <= min_spline_transition_width) return std::unexpected(warp_error_t::invalid_offset_width);

            auto const c_L = *offset_center;
            auto const k_L = erf_sharpness_from_width(offset_width);
            offset_half.emplace(c_L, k_L);

            auto const phi_at_onset_end
                = offset_half->integral(offset_half->band_hi()) - offset_half->integral(offset_half->band_lo());

            lag = offset_half->band_hi() - phi_at_onset_end;
        }

        // --- limit inversion: leftmost x where g reaches the limit ---
        auto const g = [&prev](real_t x) noexcept { return prev(x); };
        auto const crossing = detail::bisect_crossing(domain_lo, domain_hi, limit, g);

        auto limit_half = std::optional<half_t>{};
        auto phi_at_b2 = real_t{0};
        auto phi_capped = real_t{0};

        if (crossing.has_value())
        {
            auto const x_cross = *crossing;

            auto const k_R = -erf_sharpness_from_width(limit_width);
            auto const c_R = x_cross + lag;
            limit_half.emplace(c_R, k_R);

            phi_at_b2 = limit_half->band_lo() - lag;

            if (offset_half.has_value() && offset_half->band_hi() > limit_half->band_lo())
            {
                return std::unexpected(warp_error_t::transition_overlap);
            }

            phi_capped = phi_at_b2
                + (limit_half->integral(limit_half->band_hi()) - limit_half->integral(limit_half->band_lo()));

            auto const tolerance = real_t{16} * std::numeric_limits<real_t>::epsilon()
                * (real_t{1} + std::abs(x_cross) + lag + limit_half->saturation_half_width());
            if (std::abs(phi_capped - x_cross) > tolerance)
            {
                return std::unexpected(warp_error_t::limit_placement_failure);
            }
        }

        auto config = config_t{
            .offset = std::move(offset_half),
            .limit = std::move(limit_half),
            .lag = lag,
            .phi_at_b2 = phi_at_b2,
            .phi_capped = phi_capped,
        };
        return warp_t{std::move(config), std::move(prev)};
    }
};

struct quadratic_t
{
    template <typename value_t> constexpr auto operator()(value_t x) const noexcept -> value_t { return x * x; }
};

TEST(signal_chain_test, integration)
{
    using real_t = float64_t;

    auto const base_curve = quadratic_t{};
    auto const y0 = real_t{0.25};
    auto const output_scale = real_t{1.5};
    auto const limit = real_t{17.0};
    auto const limit_width = real_t{0.5};
    auto const offset_center = real_t{0.5};
    auto const offset_width = real_t{0.25};
    auto const domain_lo = real_t{0.0};
    auto const domain_hi = real_t{256.0};

    constexpr auto min_spline_transition_width = signal_chain::min_spline_transition_width<real_t>;
    auto const output_chain_result = domain_warp_factory_t<real_t, min_spline_transition_width>{}(
        make_output_offset(y0, make_output_scale(output_scale, base_curve)), limit, limit_width,
        std::make_optional(offset_center), offset_width, domain_lo, domain_hi);
    assert(output_chain_result.has_value());
    auto const& output_chain = *output_chain_result;

    auto const dx = static_cast<real_t>(0.1);
    for (auto x = 0.0; x < static_cast<real_t>(5.0) + dx * 3; x += dx)
    {
        auto const y = output_chain(jet_t{x, 1.0});
        std::cout << x << ", (" << primal(y) << ", " << tangent(y) << ")\n";
    }
    std::cout.flush();
}

} // namespace crv::signal_chain
