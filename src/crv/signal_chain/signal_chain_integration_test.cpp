// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/complex_traits.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/signal_chain/curves/derivatives.hpp>
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

using model::curves::derivatives_t;

template <std::floating_point real_t> inline constexpr real_t min_spline_transition_width = real_t{0.01};

// --------------------------------------------------------------------------------------------------------------------h

enum class warp_error_t
{
    invalid_limit_width,
    invalid_offset_width,
    transition_overlap,
    limit_placement_failure
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

struct compose_derivatives_t
{
    template <int order, typename scalar_t>
    constexpr auto operator()(derivatives_t<order, scalar_t> const& outer,
        derivatives_t<order, scalar_t> const& inner) const noexcept -> derivatives_t<order, scalar_t>
    {
        static_assert(order >= 0 && order <= 3, "compose_derivatives_t supports orders 0..3");

        auto out = derivatives_t<order, scalar_t>{};

        out.f = outer.f;

        if constexpr (order >= 1) { out.d1 = outer.d1 * inner.d1; }
        if constexpr (order >= 2) { out.d2 = outer.d2 * inner.d1 * inner.d1 + outer.d1 * inner.d2; }
        if constexpr (order >= 3)
        {
            out.d3 = outer.d3 * inner.d1 * inner.d1 * inner.d1 + scalar_t{3} * outer.d2 * inner.d1 * inner.d2
                + outer.d1 * inner.d3;
        }

        return out;
    }
};

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

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        using std::exp;
        using std::real;

        static_assert(order >= 0 && order <= 3, "erf_half_t supports orders 0..3");

        auto const sat = erf_saturation_z<real_t>;
        auto const z = static_cast<scalar_t>(k_) * (x - static_cast<scalar_t>(center_));

        auto d = derivatives_t<order, scalar_t>{};

        if (real(z) <= -sat) { return d; }
        if (real(z) >= sat)
        {
            d.f = scalar_t{1};
            return d;
        }

        d.f = scalar_t{0.5} * (scalar_t{1} + erf_(z));
        if constexpr (order == 0) return d;
        else
        {
            auto const k = static_cast<scalar_t>(k_);
            auto const e = exp(-(z * z));
            auto const inv_sqrt_pi = static_cast<scalar_t>(inv_sqrt_pi_);

            auto const g1 = inv_sqrt_pi * e;
            if constexpr (order >= 1) d.d1 = k * g1;

            auto const g2 = scalar_t{-2} * inv_sqrt_pi * z * e;
            if constexpr (order >= 2) d.d2 = (k * k) * g2;

            auto const g3 = scalar_t{2} * inv_sqrt_pi * (scalar_t{2} * z * z - scalar_t{1}) * e;
            if constexpr (order >= 3) d.d3 = (k * k * k) * g3;

            return d;
        }
    }

    template <typename scalar_t> constexpr auto integral(scalar_t x) const noexcept -> scalar_t
    {
        using std::exp;

        auto const z = static_cast<scalar_t>(k_) * (x - static_cast<scalar_t>(center_));
        auto const k = static_cast<scalar_t>(k_);
        auto const inv_sqrt_pi = static_cast<scalar_t>(inv_sqrt_pi_);

        auto const G = scalar_t{0.5} * z * (scalar_t{1} + erf_(z)) + scalar_t{0.5} * inv_sqrt_pi * exp(-(z * z));
        return G / k;
    }

private:
    template <typename scalar_t> static auto erf_(scalar_t z) -> scalar_t
    {
        using std::exp;
        if constexpr (std::floating_point<scalar_t>)
        {
            using std::erf;
            return erf(z);
        }
        else
        {
            using value_t = real_type_t<scalar_t>;
            auto const a = z.real();
            auto const b = z.imag();
            using std::erf;
            return scalar_t{erf(a), b * static_cast<value_t>(two_over_sqrt_pi_) * exp(-a * a)};
        }
    }

    static constexpr real_t inv_sqrt_pi_ = std::numbers::inv_sqrtpi_v<real_t>;
    static constexpr real_t two_over_sqrt_pi_ = real_t{2} * std::numbers::inv_sqrtpi_v<real_t>;

    real_t center_;
    real_t k_;
    real_t half_width_;
};

template <typename real_t, typename prev_t> struct input_scale_t
{
    real_t scale;
    prev_t prev;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return prev(x * static_cast<scalar_t>(scale));
    }

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = prev.template derivatives<order>(x * static_cast<scalar_t>(scale));
        auto const s = static_cast<scalar_t>(scale);
        if constexpr (order >= 1) d.d1 *= s;
        if constexpr (order >= 2) d.d2 *= (s * s);
        if constexpr (order >= 3) d.d3 *= (s * s * s);
        return d;
    }
};

template <typename real_t, typename prev_t> struct input_offset_t
{
    real_t offset;
    prev_t prev;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return prev(x - static_cast<scalar_t>(offset));
    }

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        return prev.template derivatives<order>(x - static_cast<scalar_t>(offset));
    }
};

template <typename real_t, typename prev_t> struct output_scale_t
{
    real_t scale;
    prev_t prev;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return prev(x) * static_cast<scalar_t>(scale);
    }

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = prev.template derivatives<order>(x);
        auto const s = static_cast<scalar_t>(scale);
        d.f *= s;
        if constexpr (order >= 1) d.d1 *= s;
        if constexpr (order >= 2) d.d2 *= s;
        if constexpr (order >= 3) d.d3 *= s;
        return d;
    }
};

template <typename real_t, typename prev_t> struct output_offset_t
{
    real_t offset;
    prev_t prev;

    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return prev(x) + static_cast<scalar_t>(offset);
    }

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = prev.template derivatives<order>(x);
        d.f += static_cast<scalar_t>(offset);
        return d;
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

template <std::floating_point t_real_t, typename t_prev_t, typename t_compose_t> class domain_warp_t
{
public:
    using real_t = t_real_t;
    using prev_t = t_prev_t;
    using compose_t = t_compose_t;
    using half_t = erf_half_t<real_t>;

    struct config_t
    {
        std::optional<half_t> offset;
        std::optional<half_t> limit;
        real_t lag = real_t{0};
        real_t phi_at_b2 = real_t{0};
        real_t phi_capped = real_t{0};
    };

    domain_warp_t(config_t config, prev_t prev, compose_t compose = {}) noexcept
        : config_{std::move(config)}, prev_{std::move(prev)}, compose_{std::move(compose)}
    {}

    constexpr auto operator()(real_t x) const noexcept -> real_t { return derivatives<0>(x).f; }

    constexpr auto operator()(jet_t<real_t> x) const noexcept -> crv::jet_t<real_t>
    {
        auto const d = derivatives<1>(primal(x));
        return crv::jet_t<real_t>{d.f, d.d1 * tangent(x)};
    }

    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        if (!config_.offset.has_value() && !config_.limit.has_value()) { return prev_.template derivatives<order>(x); }

        auto const inner = warp_derivatives<order>(x);
        auto const outer = prev_.template derivatives<order>(inner.f);
        return compose_.template operator()<order>(outer, inner);
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

    template <typename scalar_t> constexpr auto classify(scalar_t x) const noexcept -> region_t
    {
        using std::real;
        auto const rx = real(x);
        auto const& c = config_;

        if (c.offset.has_value())
        {
            if (rx <= c.offset->band_lo()) return region_t::paused;
            if (rx < c.offset->band_hi()) return region_t::onset;
        }

        if (c.limit.has_value())
        {
            if (rx < c.limit->band_lo()) return region_t::plateau;
            if (rx < c.limit->band_hi()) return region_t::limit;
            return region_t::capped;
        }

        return region_t::plateau;
    }

    template <int order, typename scalar_t>
    constexpr auto warp_derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = derivatives_t<order, scalar_t>{};

        switch (classify(x))
        {
            case region_t::paused: return d;

            case region_t::plateau:
            {
                d.f = x - static_cast<scalar_t>(config_.lag);
                if constexpr (order >= 1) d.d1 = scalar_t{1};
                return d;
            }

            case region_t::capped: d.f = static_cast<scalar_t>(config_.phi_capped); return d;

            case region_t::onset: return onset_jet<order>(x);

            case region_t::limit: return limit_jet<order>(x);
        }

        return d;
    }

    template <int order, typename scalar_t>
    constexpr auto onset_jet(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto const& half = *config_.offset;
        auto d = half.template derivatives<order>(x);
        shift_down_one_order<order>(d);
        d.f = half.template integral<scalar_t>(x) - half.template integral(static_cast<scalar_t>(half.band_lo()));
        return d;
    }

    template <int order, typename scalar_t>
    constexpr auto limit_jet(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto const& half = *config_.limit;
        auto d = half.template derivatives<order>(x);
        shift_down_one_order<order>(d);
        d.f = static_cast<scalar_t>(config_.phi_at_b2)
            + (half.template integral<scalar_t>(x) - half.template integral(static_cast<scalar_t>(half.band_lo())));
        return d;
    }

    template <int order, typename scalar_t>
    constexpr auto shift_down_one_order(derivatives_t<order, scalar_t>& d) const noexcept -> void
    {
        if constexpr (order >= 3) d.d3 = d.d2;
        if constexpr (order >= 2) d.d2 = d.d1;
        if constexpr (order >= 1) d.d1 = d.f;
    }

    config_t config_;
    prev_t prev_;
    [[no_unique_address]] compose_t compose_;
};

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

template <std::floating_point real_t, typename compose_t, real_t min_spline_transition_width>
struct domain_warp_factory_t
{
    template <typename prev_t>
    [[nodiscard]] auto operator()(prev_t prev, real_t limit, real_t limit_width, std::optional<real_t> offset_center,
        real_t offset_width, real_t domain_lo, real_t domain_hi, compose_t compose = {})
        -> std::expected<domain_warp_t<real_t, prev_t, compose_t>, warp_error_t>
    {
        using warp_t = domain_warp_t<real_t, prev_t, compose_t>;
        using half_t = erf_half_t<real_t>;
        using config_t = typename warp_t::config_t;

        if (limit_width <= min_spline_transition_width) return std::unexpected(warp_error_t::invalid_limit_width);

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

        auto config = config_t{.offset = std::move(offset_half),
            .limit = std::move(limit_half),
            .lag = lag,
            .phi_at_b2 = phi_at_b2,
            .phi_capped = phi_capped};

        return warp_t{std::move(config), std::move(prev), std::move(compose)};
    }
};

struct quadratic_t
{
    template <typename scalar_t> constexpr auto operator()(scalar_t x) const noexcept -> scalar_t { return x * x; }
    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = derivatives_t<order, scalar_t>{};
        d.f = x * x;
        if constexpr (order >= 1) d.d1 = scalar_t{2} * x;
        if constexpr (order >= 2) d.d2 = scalar_t{2};
        if constexpr (order >= 3) d.d3 = scalar_t{0};
        return d;
    }
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
    auto const output_chain_result
        = domain_warp_factory_t<real_t, compose_derivatives_t, min_spline_transition_width>{}(
            make_output_offset(y0, make_output_scale(output_scale, base_curve)), limit, limit_width,
            std::make_optional(offset_center), offset_width, domain_lo, domain_hi);
    assert(output_chain_result.has_value());
    auto const& output_chain = *output_chain_result;

    auto const dx = static_cast<real_t>(0.1);
    for (auto x = jet_t<real_t>{0.0, 1.0}; x.f < static_cast<real_t>(5.0) + dx * 3; x += dx)
    {
        auto const y = output_chain(x);
        std::cout << x.f << ", (" << y.f << ", " << y.df << ")\n";
    }
    std::cout.flush();
}

} // namespace crv::signal_chain
