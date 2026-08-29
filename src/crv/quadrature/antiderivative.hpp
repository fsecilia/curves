// SPDX-License-Identifier: MIT

/// \file
/// \brief cached antiderivative and conditioned integrand-prefix evaluation using adaptive quadrature
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/math/jet/jet.hpp>
#include <algorithm>
#include <utility>
#include <vector>

namespace crv::quadrature {

/// antiderivative, yielding a 1-jet via an accumulation function
template <typename t_integral_t> class antiderivative_t
{
public:
    using integral_t = t_integral_t;
    using scalar_t = integral_t::scalar_t;

    using jet_t = jet_t<scalar_t>;
    using boundaries_t = std::vector<scalar_t>;
    using cumulative_sums_t = std::vector<scalar_t>;

    constexpr antiderivative_t(integral_t integral, boundaries_t boundaries, cumulative_sums_t cumulative_sums) noexcept
        : integral_{std::move(integral)}, boundaries_{std::move(boundaries)},
          cumulative_sums_{std::move(cumulative_sums)}
    {
        assert(!boundaries_.empty() && "antiderivative_t: empty intervals");
        assert(
            boundaries_.size() == cumulative_sums_.size() && "antiderivative_t: interval arrays must have equal sizes");
        assert(boundaries_.front() == scalar_t{0} && "antiderivative_t: origin must start at 0");
        assert(cumulative_sums_.front() == scalar_t{0} && "antiderivative_t: cumulative sum must start at 0");

        for (std::size_t i = 1; i < boundaries_.size(); ++i)
        {
            assert(boundaries_[i - 1] < boundaries_[i] && "antiderivative_t: boundaries must be strictly increasing");
        }
    }

    /// evaluates accumulation function with a scalar, returning F(x)
    constexpr auto operator()(scalar_t x) const noexcept -> scalar_t { return integrate(x); }

    /// evaluates F(x) and derivative f(x) with a jet
    ///
    /// F(x) is the nearest cached prefix plus a local residual integral. Its derivative is the original integrand
    /// evaluated directly.
    constexpr auto operator()(jet_t x) const noexcept -> jet_t
    {
        auto const primal_x = primal(x);
        return jet_t{integrate(primal_x), integral_.evaluate_integrand(primal_x) * tangent(x)};
    }

    /// evaluates the derivative of the antiderivative, which is the retained integrand
    constexpr auto derivative(scalar_t x) const noexcept -> scalar_t
    {
        assert_domain(x);
        return integral_.evaluate_integrand(x);
    }

    /// evaluates mean integrand over [0, x]
    ///
    /// Directly computing F(x) / x would magnify fixed absolute integration error near zero. Let a be the previous
    /// cached boundary, g_a = F(a) / a, and m(a,x) the mean over the uncached remainder. Then
    ///
    ///     G(x) = m(a,x) + (a/x) * (g_a - m(a,x)).
    ///
    /// Since a/x is in [0,1], this stays well conditioned. In the first interval a == 0, so only the residual mean is
    /// needed. At x == 0, the continuous mean is f(0).
    constexpr auto mean_integrand(scalar_t x) const noexcept -> scalar_t
    {
        assert_domain(x);

        if (x == scalar_t{0}) return derivative(x);

        auto const left_index = find_left_index(x);
        auto const left = boundaries_[left_index];

        // exact cache boundary needs no residual evaluation
        if (x == left) return cumulative_sums_[left_index] / left;

        auto const residual_mean = integral_.average(left, x);
        if (left == scalar_t{0}) return residual_mean;

        auto const cached_prefix_mean = cumulative_sums_[left_index] / left;
        auto const prefix_fraction = left / x;
        return residual_mean + prefix_fraction * (cached_prefix_mean - residual_mean);
    }

    /// right edge of the cached domain
    constexpr auto domain_end() const noexcept -> scalar_t { return boundaries_.back(); }

    /// number of accepted quadrature segments; cache also stores the origin
    constexpr auto segment_count() const noexcept -> int_t { return static_cast<int_t>(boundaries_.size() - 1); }

private:
    constexpr auto integrate(scalar_t x) const noexcept -> scalar_t
    {
        assert_domain(x);

        auto const left_index = find_left_index(x);
        auto const residual = integral_.integrate(boundaries_[left_index], x);
        return cumulative_sums_[left_index] + residual;
    }

    constexpr auto assert_domain([[maybe_unused]] scalar_t x) const noexcept -> void
    {
        assert(boundaries_.front() <= x && x <= boundaries_.back() && "antiderivative_t: domain error");
    }

    constexpr auto find_left_index(scalar_t x) const noexcept -> std::size_t
    {
        // x >= front(), so upper_bound() is always past begin()
        auto const right = std::ranges::upper_bound(boundaries_, x);
        return static_cast<std::size_t>(right - boundaries_.begin() - 1);
    }

    integral_t integral_;
    boundaries_t boundaries_;
    cumulative_sums_t cumulative_sums_;
};

/// The standalone result of an adaptive integration pass
template <typename t_antiderivative_t> struct integration_result_t
{
    using antiderivative_t = t_antiderivative_t;
    using scalar_t = antiderivative_t::scalar_t;

    antiderivative_t antiderivative;
    scalar_t achieved_error;
    scalar_t max_error;
    bool refinement_limited = false;

    auto operator<=>(integration_result_t const&) const noexcept -> auto = default;
    auto operator==(integration_result_t const&) const noexcept -> bool = default;
};

namespace generic {

/// accumulates quadrature results and assembles final antiderivative
template <typename t_accumulator_t, typename t_antiderivative_t> class antiderivative_builder_t
{
public:
    using accumulator_t = t_accumulator_t;
    using antiderivative_t = t_antiderivative_t;
    using scalar_t = antiderivative_t::scalar_t;
    using integral_t = antiderivative_t::integral_t;
    using result_t = integration_result_t<antiderivative_t>;

    constexpr antiderivative_builder_t()
    {
        boundaries_.reserve(32);
        boundaries_.push_back(scalar_t{0});

        cumulative_sums_.reserve(32);
        cumulative_sums_.push_back(scalar_t{0});
    }

    constexpr auto append(scalar_t right_bound, scalar_t area, scalar_t error, bool refinement_limited = false) -> void
    {
        assert(right_bound > boundaries_.back()
            && "antiderivative_builder_t: boundaries must be monotonically increasing");

        running_area_ += area;
        running_error_ += error;
        max_error_ = max(max_error_, error);
        refinement_limited_ |= refinement_limited;

        boundaries_.push_back(right_bound);
        cumulative_sums_.push_back(static_cast<scalar_t>(running_area_));
    }

    constexpr auto finalize(integral_t integral) && noexcept -> result_t
    {
        return result_t{
            antiderivative_t{
                std::move(integral),
                std::move(boundaries_),
                std::move(cumulative_sums_),
            },
            static_cast<scalar_t>(running_error_),
            max_error_,
            refinement_limited_,
        };
    }

private:
    using boundaries_t = antiderivative_t::boundaries_t;
    using cumulative_sums_t = antiderivative_t::cumulative_sums_t;

    boundaries_t boundaries_{};
    cumulative_sums_t cumulative_sums_{};
    accumulator_t running_area_{};
    accumulator_t running_error_{};
    scalar_t max_error_{0};
    bool refinement_limited_{false};
};

} // namespace generic

template <typename integral_t>
using antiderivative_builder_t
    = generic::antiderivative_builder_t<compensated_accumulator_t<typename integral_t::scalar_t>,
        antiderivative_t<integral_t>>;

} // namespace crv::quadrature
