// SPDX-License-Identifier: MIT

/// \file
/// \brief accumulation function yielding a 1-jet using adaptive quadrature
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/math/jet/jet.hpp>
#include <flat_map>
#include <utility>

namespace crv::quadrature {

/// antiderivative, yielding a 1-jet via an accumulation function
template <typename t_real_t, typename t_integral_t> class antiderivative_t
{
public:
    using real_t     = t_real_t;
    using integral_t = t_integral_t;

    using jet_t             = jet_t<real_t>;
    using map_t             = std::flat_map<real_t, real_t>;
    using boundaries_t      = map_t::key_container_type;
    using cumulative_sums_t = map_t::mapped_container_type;

    constexpr antiderivative_t(integral_t integral, map_t intervals) noexcept
        : integral_{std::move(integral)}, intervals_{std::move(intervals)}
    {
        assert(!intervals_.empty() && "antiderivative_t: empty intervals");
        assert(intervals_.begin()->first == real_t{0} && "antiderivative_t: origin must start at 0");
        assert(intervals_.begin()->second == real_t{0} && "antiderivative_t: cumulative sum must start at 0");
    }

    /// evaluates accumulation function F(x) and its derivative f(x).
    ///
    /// The primal of the integral is the sum of the nearest cached base integral and a local residual calculated using
    /// the quadrature rule and integrand. The tangent of the integral, by the First Fundamental Theorem of Calculus, is
    /// the original integrand itself, evaluated directly.
    constexpr auto operator()(real_t location) const noexcept -> jet_t
    {
        assert(intervals_.keys().front() <= location && location <= intervals_.keys().back()
               && "antiderivative_t: domain error");

        auto const right    = intervals_.upper_bound(location);
        auto const left     = std::ranges::prev(right);
        auto const residual = integral_.integrate(left->first, location);
        auto const integral = left->second + residual;

        return jet_t{integral, integral_.evaluate_integrand(location)};
    }

    /// number of accepted quadrature segments
    ///
    /// the interval map always carries the origin plus one entry per accepted segment, so the count is size - 1
    constexpr auto segment_count() const noexcept -> int_t { return static_cast<int_t>(intervals_.size() - 1); }

private:
    integral_t integral_;
    map_t      intervals_;
};

namespace generic {

/// accumulates quadrature results and assembles final antiderivative
template <std::floating_point real_t, typename accumulator_t, typename antiderivative_t> class antiderivative_builder_t
{
public:
    using integral_t = antiderivative_t::integral_t;

    struct result_t
    {
        antiderivative_t antiderivative;
        real_t           achieved_error;
        real_t           max_error;
    };

    constexpr antiderivative_builder_t()
    {
        boundaries_.reserve(32);
        boundaries_.push_back(real_t{0});

        cumulative_sums_.reserve(32);
        cumulative_sums_.push_back(real_t{0});
    }

    constexpr auto append(real_t right_bound, real_t area, real_t error) -> void
    {
        assert(right_bound > boundaries_.back()
               && "antiderivative_builder_t: boundaries must be monotonically increasing");

        running_area_ += area;
        running_error_ += error;
        max_error_ = std::max(max_error_, error);

        boundaries_.push_back(right_bound);
        cumulative_sums_.push_back(static_cast<real_t>(running_area_));
    }

    constexpr auto finalize(integral_t integral) && noexcept -> result_t
    {
        using map_t    = antiderivative_t::map_t;
        auto intervals = map_t{std::sorted_unique, std::move(boundaries_), std::move(cumulative_sums_)};

        return result_t{
            antiderivative_t{std::move(integral), std::move(intervals)},
            static_cast<real_t>(running_error_),
            max_error_,
        };
    }

private:
    using boundaries_t      = antiderivative_t::boundaries_t;
    using cumulative_sums_t = antiderivative_t::cumulative_sums_t;

    boundaries_t      boundaries_{};
    cumulative_sums_t cumulative_sums_{};
    accumulator_t     running_area_{};
    accumulator_t     running_error_{};
    real_t            max_error_{0};
};

} // namespace generic

template <std::floating_point real_t, typename integral_t>
using antiderivative_builder_t = generic::antiderivative_builder_t<real_t, compensated_accumulator_t<real_t>,
                                                                   antiderivative_t<real_t, integral_t>>;

} // namespace crv::quadrature
