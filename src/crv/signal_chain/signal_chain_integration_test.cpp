// SPDX-License-Identifier: MIT

/// \file
/// \brief domain-warp signal-chain stage and its supporting pieces
///
/// The chain shapes a mouse-acceleration transfer function. Reading outermost-to-innermost on the input side: the
/// domain warp warps x, forwarding prev(phi(x)); prev is the output-shaped curve (output scale then offset applied over
/// the input-shaped base curve). The warp pauses x at the origin, gently unpauses across an optional offset transition,
/// runs at unit rate, then gently pauses again across a mandatory-but-placeable limit transition, freezing at a
/// ceiling. Each transition is a single erf half; phi is monotone and C-infinity, so derivatives compose by the plain
/// chain rule with no per-region special cases.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/complex_traits.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/signal_chain/curves/derivatives.hpp>
#include <crv/signal_chain/curves/traits.hpp>
#include <crv/signal_chain/quadrature/adaptive_integrator.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <iostream>
#include <limits>
#include <numbers>
#include <numeric>
#include <optional>
#include <utility>

namespace crv::signal_chain {

using model::curves::derivatives_t;

// --------------------------------------------------------------------------------------------------------------------h

// =============================================================================
// chain-rule composition of derivative jets
// =============================================================================

/// composes two truncated derivative jets under the chain rule
///
/// Given outer = derivatives of g evaluated at y, and inner = derivatives of phi evaluated at x,
/// where y = phi(x) = inner.f, returns the derivatives of (g . phi) at x.
///
/// This is the chain rule; for orders >= 2 it is Faa di Bruno's formula specialized to a single
/// inner function. inner.f is the inner *value* (already consumed to evaluate outer at y); only
/// inner.d1, d2, d3 (phi', phi'', phi''') take part in the arithmetic. The flat-region degeneracies
/// of a domain warp fall out for free: inner = {y,0,0,0} (paused, phi'=0) yields {g,0,0,0}, and
/// inner = {y,1,0,0} (running, phi'=1) yields {g,g',g'',g'''} -- pure passthrough. So consuming
/// nodes never branch the chain rule per region.
///
/// Injected into nodes as a template parameter so the chain-rule shape is an explicit, substitutable
/// dependency. Stateless; declare members [[no_unique_address]] so it costs no storage.
struct compose_derivatives_t
{
    template <int order, typename scalar_t>
    constexpr auto operator()(derivatives_t<order, scalar_t> const& outer,
        derivatives_t<order, scalar_t> const& inner) const noexcept -> derivatives_t<order, scalar_t>
    {
        static_assert(order >= 0 && order <= 3, "compose_derivatives_t supports orders 0..3");

        auto out = derivatives_t<order, scalar_t>{};

        out.f = outer.f; // g(phi(x))

        if constexpr (order >= 1)
        {
            // g' phi'
            out.d1 = outer.d1 * inner.d1;
        }
        if constexpr (order >= 2)
        {
            // g'' phi'^2 + g' phi''
            out.d2 = outer.d2 * inner.d1 * inner.d1 + outer.d1 * inner.d2;
        }
        if constexpr (order >= 3)
        {
            // g''' phi'^3 + 3 g'' phi' phi'' + g' phi'''
            out.d3 = outer.d3 * inner.d1 * inner.d1 * inner.d1 + scalar_t{3} * outer.d2 * inner.d1 * inner.d2
                + outer.d1 * inner.d3;
        }

        return out;
    }
};

// =============================================================================
// erf saturation threshold and width->sharpness mapping
// =============================================================================

// erf saturation threshold for real_t: |z| beyond which erfc(z) < epsilon, so erf rounds to +-1.
// Tight bound from the dominant term: erfc(z) ~ e^{-z^2}/(z sqrt pi), and e^{-z^2} < eps gives
// z > sqrt(-ln eps). The 1/(z sqrt pi) prefactor only shrinks erfc further, so this is exact to the
// e^{-z^2} term and a hair conservative overall -- the safe direction: never treat the tail as flat
// too soon, which would corrupt the float128 error reference.
//
// Tracks the type (double ~6, float ~4, float128 ~8.8); a fixed constant would over-saturate the
// wide types. NOT constexpr today only because libstdc++/libc++ lack constexpr sqrt/log -- promote
// when they land. Computed once per type at static-init. Moves to the common math header with erf_.
template <std::floating_point real_t>
inline real_t const erf_saturation_z = [] {
    using std::log;
    using std::sqrt;
    return sqrt(-log(std::numeric_limits<real_t>::epsilon()));
}();

// width-to-sharpness mapping for the erf transition.
//
//   k = (30 sqrt pi)^(1/3) / w  ~= 3.762 / w
//
// The user gives a transition WIDTH w; erf needs a SHARPNESS k. We do NOT map against erf's
// machine-flat saturation band (~12/k wide) -- erf packs its curvature into a far narrower central
// region, which would make the corner too sharp for the Hermite grid to resolve without extra
// knots. Instead we match erf's corner to SMOOTHERSTEP's at the same width (the polynomial
// transition we already know splines cleanly), equating the fourth-derivative peak -- the quantity
// that drives cubic-Hermite reproduction error:
//
//   smootherstep:  phi'''' _max = W'''(0)/w^3 = 60 / w^3       (W = 10t^3 - 15t^4 + 6t^5)
//   erf:           phi'''' _max = 2 k^3 / sqrt pi              (peak at band center)
//
//   2 k^3 / sqrt pi = 60 / w^3  =>  k = (30 sqrt pi)^(1/3) / w  ~= 3.762 / w
//
// With this k the erf warp inherits smootherstep's splineability: any grid + min-width floor that
// handles smootherstep handles this. (Matching peak curvature phi'' instead gives ~3.32/w; we use
// the phi'''' match -- it targets spline error directly and is marginally more conservative.)
template <std::floating_point real_t> [[nodiscard]] constexpr auto erf_sharpness_from_width(real_t w) noexcept -> real_t
{
    // (30 sqrt pi)^(1/3) as a literal, so this stays constexpr without a runtime cbrt/sqrt.
    // 30 * 1.7724538509055160273 = 53.173615527165..., cube root = 3.76237506018...
    constexpr real_t cube_root_30_sqrt_pi = static_cast<real_t>(3.7623750601800463L);
    return cube_root_30_sqrt_pi / w;
}

// =============================================================================
// one half of the warp velocity profile
// =============================================================================

/// one half of the domain-warp velocity profile: a single erf sigmoid and its integral
///
/// In its own x-frame this evaluates phi'(x) = 1/2 (1 + erf(k (x - c))) plus derivatives and the
/// antiderivative. k carries the reflection: k > 0 rises (0 -> 1), k < 0 falls (1 -> 0), since
/// erf(-z) = -erf(z). The warp instantiates this twice -- a rising half for the onset, a falling
/// half (k < 0) for the limit -- and stitches the regions between them.
///
/// erf has no compact support, so there is no normalized [0,1] frame; the natural variable is
/// z = k (x - c), unbounded, saturating to machine-flat by |z| ~ erf_saturation_z. The half owns
/// its saturation band [center - half_width, center + half_width] in x-units, so the warp reads
/// region boundaries off the halves directly rather than duplicating them.
template <std::floating_point t_real_t> class erf_half_t
{
public:
    using real_t = t_real_t;

    /// \param center  x where the sigmoid is half-risen (z = 0)
    /// \param k       sharpness; sign encodes direction (k > 0 rising, k < 0 falling). k != 0.
    erf_half_t(real_t center, real_t k) noexcept
        : center_{center}, k_{k}, half_width_{erf_saturation_z<real_t> / std::abs(k)}
    {}

    [[nodiscard]] constexpr auto center() const noexcept -> real_t { return center_; }
    [[nodiscard]] constexpr auto k() const noexcept -> real_t { return k_; }

    /// half-width in x-units beyond which this sigmoid is machine-flat (cached at construction)
    [[nodiscard]] constexpr auto saturation_half_width() const noexcept -> real_t { return half_width_; }

    /// saturation band in x: [band_lo, band_hi]. Outside it the half is flat to precision.
    [[nodiscard]] constexpr auto band_lo() const noexcept -> real_t { return center_ - half_width_; }
    [[nodiscard]] constexpr auto band_hi() const noexcept -> real_t { return center_ + half_width_; }

    /// value and derivatives of phi' = 1/2 (1 + erf(k (x - c))), in the x-frame
    ///
    /// scalar_t may be real or complex (complex-step); the saturation branch tests real(z) so an
    /// infinitesimal imaginary part never flips the region, exactly as the base curves do.
    template <int order, typename scalar_t>
    constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        using std::exp;
        using std::real;

        static_assert(order >= 0 && order <= 3, "erf_half_t supports orders 0..3");

        auto const sat = erf_saturation_z<real_t>;
        auto const z = static_cast<scalar_t>(k_) * (x - static_cast<scalar_t>(center_));

        auto d = derivatives_t<order, scalar_t>{};

        // saturated tails: phi' is exactly 0 (z <= -sat) or 1 (z >= +sat); higher derivatives 0.
        // Branch on real(z) to stay holomorphic under complex-step.
        if (real(z) <= -sat)
        {
            return d; // f and all derivatives 0 by default construction
        }
        if (real(z) >= sat)
        {
            d.f = scalar_t{1};
            return d; // higher orders remain 0
        }

        // active region. phi' = 1/2 (1 + erf(z)).
        d.f = scalar_t{0.5} * (scalar_t{1} + erf_(z));
        if constexpr (order == 0) return d;
        else
        {
            // x-frame derivatives. With g(z) = 1/2 (1 + erf(z)):
            //   g'(z)   =  (1/sqrt pi) e^{-z^2}
            //   g''(z)  = -(2/sqrt pi) z e^{-z^2}
            //   g'''(z) =  (2/sqrt pi) (2 z^2 - 1) e^{-z^2}
            // and z = k (x - c) contributes powers of k: phi'_x = k g', phi''_x = k^2 g'', etc.
            auto const k = static_cast<scalar_t>(k_);
            auto const e = exp(-(z * z));
            auto const inv_sqrt_pi = static_cast<scalar_t>(inv_sqrt_pi_);

            auto const g1 = inv_sqrt_pi * e; // g'(z)
            if constexpr (order >= 1) d.d1 = k * g1;

            auto const g2 = scalar_t{-2} * inv_sqrt_pi * z * e; // g''(z)
            if constexpr (order >= 2) d.d2 = (k * k) * g2;

            auto const g3 = scalar_t{2} * inv_sqrt_pi * (scalar_t{2} * z * z - scalar_t{1}) * e; // g'''(z)
            if constexpr (order >= 3) d.d3 = (k * k * k) * g3;

            return d;
        }
    }

    /// antiderivative of phi' over x
    ///
    /// Z-frame: integral of g(z) dz = 1/2 z (1 + erf(z)) + e^{-z^2}/(2 sqrt pi). Change of variable
    /// back to x divides by k. The additive constant is the standard form (symmetric center near
    /// the origin); the warp uses only *differences* of this (definite integrals over a band), so
    /// the constant always cancels.
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
    // partial complex-step erf, matching the base-curve convention: real scalar_t delegates to the
    // standard erf; complex scalar_t returns the true erf in the real part and the exact analytic
    // first-order tangent b (2/sqrt pi) e^{-a^2} in the imaginary part. Correct only for the
    // infinitesimal-imaginary inputs complex-step produces. (Hoists to the common math header with
    // erf_saturation_z in the eventual refactor.)
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
    real_t k_; // sign encodes direction
    real_t half_width_; // cached erf_saturation_z / |k|
};

// =============================================================================
// affine input/output stages
// =============================================================================

/// scales input before forwarding: prev(scale * x)
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

/// offsets input before forwarding: prev(x - offset). A pure shift, so derivatives are unchanged.
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

/// scales output: prev(x) * scale. Linear, so every order scales.
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

/// offsets output: prev(x) + offset. A pure value shift, so only the 0th order changes.
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
        d.f += static_cast<scalar_t>(offset); // shifts only the value
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

// =============================================================================
// domain warp stage
// =============================================================================

/// domain warp stage
///
/// Warps the input x before forwarding to prev: forwards prev(phi(x)). phi is the integral of a
/// velocity profile that is 0 (paused), rises across the optional offset transition, holds at 1
/// (plateau), falls across the optional-but-placeable limit transition, then 0 (capped). Each
/// transition is a single erf half. The two halves are independently optional:
///   offset + limit -> paused / onset / plateau / limit / capped
///   limit only     -> running-from-origin / limit / capped
///   offset only    -> paused / onset / plateau-to-end          (limit never reached)
///   neither        -> identity (phi = x); derivatives short-circuit to prev directly
///
/// phi is monotone and C-infinity (erf halves are smooth, their saturated joins machine-flat to
/// both sides), so derivatives compose by the plain chain rule with no per-region special cases --
/// compose absorbs the phi'=0 and phi'=1 degeneracies by formula.
///
/// Construct via make_domain_warp, which runs the one-time limit inversion and places the halves.
template <std::floating_point t_real_t, typename t_prev_t, typename t_compose_t = compose_derivatives_t>
class domain_warp_t
{
public:
    using real_t = t_real_t;
    using prev_t = t_prev_t;
    using compose_t = t_compose_t;
    using half_t = erf_half_t<real_t>;

    /// Region boundaries are NOT stored here -- they live on the halves (band_lo/band_hi). Only the
    /// stitch constants live here, and each is meaningful exactly when its half is present:
    ///   lag        = offset->center() (or 0 when offset absent)  -- domain lag from the pause
    ///   phi_at_b2  = limit->band_lo() - lag                      -- phi entering the limit band
    ///   phi_capped = the inversion crossing x_cross              -- frozen ceiling input
    struct config_t
    {
        std::optional<half_t> offset; // rising; absent => running from origin at slope 1
        std::optional<half_t> limit; // falling (k < 0); absent => limit never engaged
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
        // whole-warp identity: no transitions placed, phi = x. Skip the (provably passthrough)
        // compose entirely and return prev's jet directly.
        if (!config_.offset.has_value() && !config_.limit.has_value()) { return prev_.template derivatives<order>(x); }

        auto const inner = warp_derivatives<order>(x); // {phi, phi', phi'', phi'''}
        auto const outer = prev_.template derivatives<order>(inner.f); // prev's jet at y = phi(x)
        return compose_.template operator()<order>(outer, inner);
    }

private:
    enum class region_t
    {
        paused, // phi = 0
        onset, // phi rises via the offset half
        plateau, // phi = x - lag (or phi = x when offset absent, lag = 0)
        limit, // phi rises more slowly via the falling limit half
        capped // phi frozen at phi_capped
    };

    // single region decision, computed once on real(x). The regions are linearly ordered in x; a
    // half that is absent simply removes its regions from the cascade. Value and all derivatives
    // are then produced from the one chosen region, so they can never diverge.
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
        // past the offset (or no offset): either plateau/running, then the limit band, then capped.
        if (c.limit.has_value())
        {
            if (rx < c.limit->band_lo()) return region_t::plateau; // running-from-origin if no offset
            if (rx < c.limit->band_hi()) return region_t::limit;
            return region_t::capped;
        }
        // limit absent: everything past the offset is the (unbounded) plateau / running region.
        return region_t::plateau;
    }

    template <int order, typename scalar_t>
    constexpr auto warp_derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto d = derivatives_t<order, scalar_t>{};

        switch (classify(x))
        {
            case region_t::paused:
                // phi = 0, all derivatives 0
                return d;

            case region_t::plateau:
            {
                // phi = x - lag, slope 1 (lag = 0 when offset absent => phi = x)
                d.f = x - static_cast<scalar_t>(config_.lag);
                if constexpr (order >= 1) d.d1 = scalar_t{1};
                return d;
            }

            case region_t::capped:
                // phi frozen at the crossing, all derivatives 0
                d.f = static_cast<scalar_t>(config_.phi_capped);
                return d;

            case region_t::onset: return onset_jet<order>(x);

            case region_t::limit: return limit_jet<order>(x);
        }

        return d; // unreachable; satisfies the compiler
    }

    // onset: phi rises from 0 via the offset half. phi value is the definite integral from the
    // band start (anchoring constant cancels in the difference); phi' and higher come from the half
    // shifted down one order (the half returns derivatives of phi', we need derivatives of phi).
    template <int order, typename scalar_t>
    constexpr auto onset_jet(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto const& half = *config_.offset;
        auto d = half.template derivatives<order>(x); // d.f = g_L(x) = phi'
        shift_down_one_order<order>(d);
        d.f = half.template integral<scalar_t>(x) - half.template integral(static_cast<scalar_t>(half.band_lo()));
        return d;
    }

    // limit: phi continues from phi_at_b2 plus the definite integral of the falling half from its
    // band start; phi' and higher come from the half shifted down one order.
    template <int order, typename scalar_t>
    constexpr auto limit_jet(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
    {
        auto const& half = *config_.limit;
        auto d = half.template derivatives<order>(x); // d.f = g_R(x) = phi'
        shift_down_one_order<order>(d);
        d.f = static_cast<scalar_t>(config_.phi_at_b2)
            + (half.template integral<scalar_t>(x) - half.template integral(static_cast<scalar_t>(half.band_lo())));
        return d;
    }

    // The half's derivatives<n> returns {g, g', g'', g'''} where g = phi'. We need
    // {phi, phi', phi'', phi'''}: phi' = g, phi'' = g', phi''' = g''. Slide each member down one
    // slot; the caller overwrites d.f with the actual phi (the integral). g''' is dropped -- phi
    // needs only up to phi''' = g'', which order-3 of the half supplies.
    template <int order, typename scalar_t>
    constexpr auto shift_down_one_order(derivatives_t<order, scalar_t>& d) const noexcept -> void
    {
        if constexpr (order >= 3) d.d3 = d.d2; // phi''' = g''
        if constexpr (order >= 2) d.d2 = d.d1; // phi''  = g'
        if constexpr (order >= 1) d.d1 = d.f; // phi'   = g
        // d.f overwritten by caller with phi (the integral)
    }

    config_t config_;
    prev_t prev_;
    [[no_unique_address]] compose_t compose_;
};

// =============================================================================
// domain warp factory
// =============================================================================

namespace detail {

// leftmost x in [lo, hi] where g(x) >= target, by bisection to fp collapse, clamped to hi.
//
// Assumes g monotone increasing on [lo, hi] -- maintained by construction throughout the chain
// (all base curves are monotone), so it is not re-checked here. The returned bool reports whether
// the target was actually reached: when g(hi) < target the crossing does not exist and we return
// {hi, false} so the caller can choose not to place the limit half. (v1 simply omits it; this is
// exactly the point that soft roll-on of the limit will later replace.)
template <std::floating_point real_t, typename g_t>
[[nodiscard]] auto bisect_crossing(real_t lo, real_t hi, real_t target, g_t const& g) noexcept
    -> std::pair<real_t, bool>
{
    if (g(hi) < target) return {hi, false}; // never reached -- ROLL-ON HOOK
    if (g(lo) >= target) return {lo, true}; // reached at or before the domain start

    while (true)
    {
        auto const mid = std::midpoint(lo, hi);
        if (mid == lo || mid == hi) break;
        if (g(mid) < target) lo = mid;
        else hi = mid;
    }
    return {hi, true};
}

} // namespace detail

/// builds a domain warp, running the one-time limit inversion against prev.
///
/// prev is the fully output-shaped curve g = scale*base + offset (input transforms already inside
/// base); the limit value is measured on g, so the inversion searches g directly. offset_width <= 0
/// (or no offset_center) disables the offset transition (running from the origin). If the limit is
/// never reached on [domain_lo, domain_hi], the limit half is omitted (v1: caps at the curve's own
/// maximum; gentle roll-on of the limit is deferred). With neither transition present the warp is
/// the identity.
template <std::floating_point real_t, typename prev_t, typename compose_t = compose_derivatives_t>
[[nodiscard]] auto make_domain_warp(prev_t prev, real_t limit, real_t limit_width, std::optional<real_t> offset_center,
    real_t offset_width, real_t domain_lo, real_t domain_hi, compose_t compose = {})
{
    using warp_t = domain_warp_t<real_t, prev_t, compose_t>;
    using half_t = erf_half_t<real_t>;
    using config_t = typename warp_t::config_t;

    assert(limit_width > real_t{0} && "make_domain_warp: limit_width must be positive");

    // --- offset placement (optional) ---
    auto offset_half = std::optional<half_t>{};
    auto lag = real_t{0};
    if (offset_center.has_value() && offset_width > real_t{0})
    {
        auto const c_L = *offset_center;
        auto const k_L = erf_sharpness_from_width(offset_width); // rising: k_L > 0
        offset_half.emplace(c_L, k_L);
        // symmetric rising sigmoid: area over its band == half_width exactly, so the lag (the
        // domain shift accumulated by pausing then unpausing) equals the offset center.
        lag = c_L;

        // onset->plateau continuity: phi at the end of the onset equals the lagged-line value at
        // the same x. onset_end = integral over the band == half_width; lagged line at band_hi is
        // band_hi - lag = (c_L + h_L) - c_L = h_L. Equal by the symmetric-area identity.
        auto const onset_end
            = offset_half->integral(offset_half->band_hi()) - offset_half->integral(offset_half->band_lo());
        auto const lagged_line_at_band_hi = offset_half->band_hi() - lag;
        assert(std::abs(onset_end - lagged_line_at_band_hi)
                <= real_t{16} * std::numeric_limits<real_t>::epsilon() * (real_t{1} + std::abs(onset_end))
            && "make_domain_warp: onset/plateau join discontinuous (symmetric-area assumption broken)");
    }

    // --- limit inversion: leftmost x where g reaches the limit ---
    auto const g = [&prev](real_t x) noexcept { return prev(x); };
    auto const [x_cross, reached] = detail::bisect_crossing(domain_lo, domain_hi, limit, g);

    auto limit_half = std::optional<half_t>{};
    auto phi_at_b2 = real_t{0};
    auto phi_capped = real_t{0};

    if (reached)
    {
        // falling half: k_R < 0. c_R = x_cross + lag (symmetric falling area == half_width, so the
        // center lands exactly at the lagged crossing; phi_capped then == x_cross by construction).
        auto const k_R = -erf_sharpness_from_width(limit_width);
        auto const c_R = x_cross + lag;
        limit_half.emplace(c_R, k_R);

        phi_at_b2 = limit_half->band_lo() - lag;

        // non-overlap: the onset must finish saturating before the limit band starts.
        assert((!offset_half.has_value() || offset_half->band_hi() <= limit_half->band_lo())
            && "make_domain_warp: offset and limit transitions overlap; the UI must keep them disjoint");

        // phi_capped == x_cross by construction (the crossing is exactly the frozen input that
        // makes g(phi_capped) == limit). Computed independently and asserted to match, as a loud
        // check that the symmetric-area placement held.
        phi_capped
            = phi_at_b2 + (limit_half->integral(limit_half->band_hi()) - limit_half->integral(limit_half->band_lo()));
        assert(std::abs(phi_capped - x_cross)
                <= real_t{16} * std::numeric_limits<real_t>::epsilon() * (real_t{1} + std::abs(x_cross))
            && "make_domain_warp: limit placement diverged from the crossing (symmetric-area broken)");
    }

    auto config = config_t{.offset = std::move(offset_half),
        .limit = std::move(limit_half),
        .lag = lag,
        .phi_at_b2 = phi_at_b2,
        .phi_capped = phi_capped};

    return warp_t{std::move(config), std::move(prev), std::move(compose)};
}

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
    // using scalar_t = real_t;

    auto const base_curve = quadratic_t{};
    auto const y0 = real_t{0.25};
    auto const output_scale = real_t{1.5};
    auto const limit = real_t{17.0};
    auto const limit_width = real_t{0.5};
    auto const offset_center = real_t{0.5};
    auto const offset_width = real_t{0.25};
    auto const domain_lo = real_t{0.0};
    auto const domain_hi = real_t{256.0};

    auto output_chain = make_domain_warp(make_output_offset(y0, make_output_scale(output_scale, base_curve)), limit,
        limit_width, std::make_optional(offset_center), offset_width, domain_lo, domain_hi);

    auto const dx = static_cast<real_t>(0.1);
    for (auto x = jet_t<real_t>{0.0, 1.0}; x.f < static_cast<real_t>(5.0) + dx * 3; x += dx)
    {
        auto y = output_chain(x);
        std::cout << x.f << ", (" << y.f << ", " << y.df << ")\n";
    }
    std::cout.flush();
}

} // namespace crv::signal_chain
