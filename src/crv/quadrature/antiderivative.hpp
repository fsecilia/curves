// SPDX-License-Identifier: MIT

/// \file
/// \brief accumulation function yielding a 1-jet using adaptive quadrature
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

    /// evaluates accumulation function with a jet, returning F(x) and its derivative f(x).
    ///
    /// The primal of the integral is the sum of the nearest cached base integral and a local residual calculated
    /// using the quadrature rule and integrand. The tangent of the integral, by the First Fundamental Theorem of
    /// Calculus, is the original integrand itself, evaluated directly.
    constexpr auto operator()(jet_t x) const noexcept -> jet_t
    {
        auto const primal_x = primal(x);
        return jet_t{integrate(primal_x), integral_.evaluate_integrand(primal_x) * tangent(x)};
    }

    /// number of accepted quadrature segments
    ///
    /// the interval map always carries the origin plus one entry per accepted segment, so the count is size - 1
    constexpr auto segment_count() const noexcept -> int_t { return static_cast<int_t>(boundaries_.size() - 1); }

private:
    constexpr auto integrate(scalar_t x) const noexcept -> scalar_t
    {
        assert(boundaries_.front() <= x && x <= boundaries_.back() && "antiderivative_t: domain error");

        // x >= front(), so upper_bound() is always past begin()
        auto const right = std::ranges::upper_bound(boundaries_, x);
        auto const left_index = static_cast<std::size_t>(right - boundaries_.begin() - 1);

        auto const residual = integral_.integrate(boundaries_[left_index], x);
        return cumulative_sums_[left_index] + residual;
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

    constexpr auto append(scalar_t right_bound, scalar_t area, scalar_t error) -> void
    {
        assert(right_bound > boundaries_.back()
            && "antiderivative_builder_t: boundaries must be monotonically increasing");

        running_area_ += area;
        running_error_ += error;
        max_error_ = max(max_error_, error);

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
};

} // namespace generic

template <typename integral_t>
using antiderivative_builder_t
    = generic::antiderivative_builder_t<compensated_accumulator_t<typename integral_t::scalar_t>,
        antiderivative_t<integral_t>>;

} // namespace crv::quadrature
