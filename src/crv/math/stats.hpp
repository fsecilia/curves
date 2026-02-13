// SPDX-License-Identifier: MIT
/**
    \file
    \brief basic statisticvs

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/arg_min_max.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/math/integer.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <map>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// Histograms
// --------------------------------------------------------------------------------------------------------------------

template <typename value_t> class histogram_t;

/// vector-based integer histogram
template <std::signed_integral value_t> class histogram_t<value_t>
{
public:
    using map_t = std::map<int_t, int_t>;

    histogram_t(map_t map) noexcept : map_{std::move(map)}
    {
        for (auto const& element : map_) count_ += element.second;
    }

    histogram_t() = default;

    auto count() const noexcept -> int_t { return count_; }

    auto sample(value_t value) -> void
    {
        map_[value]++;
        ++count_;
    }

    template <typename visitor_t> auto visit(visitor_t&& visitor) const -> void
    {
        for (auto const& element : map_)
        {
            if (!visitor(element.first, element.second)) return;
        }
    }

    friend auto operator<<(std::ostream& out, histogram_t const& src) -> std::ostream&
    {
        out << "{";

        auto first = true;
        src.visit([&](auto value, auto count) {
            if (!first) out << ", ";
            else first = false;
            out << "{" << value << ", " << count << "}";
            return true;
        });
        out << "}";
        return out;
    }

    constexpr auto operator<=>(histogram_t const&) const noexcept -> auto = default;
    constexpr auto operator==(histogram_t const&) const noexcept -> bool  = default;

private:
    int_t count_{};
    map_t map_{};
};

// --------------------------------------------------------------------------------------------------------------------
// Percentiles
// --------------------------------------------------------------------------------------------------------------------

template <typename value_t, typename histogram_t = histogram_t<value_t>> struct percentile_calculator_t
{
    struct result_t
    {
        value_t p50{};
        value_t p90{};
        value_t p95{};
        value_t p99{};
        value_t p100{};

        friend auto operator<<(std::ostream& out, result_t const& src) -> std::ostream&
        {
            return out << "p50 = " << src.p50 << ", p90 = " << src.p90 << ", p95 = " << src.p95 << ", p99 = " << src.p99
                       << ", p100 = " << src.p100;
        }

        constexpr auto operator<=>(result_t const&) const noexcept -> auto = default;
        constexpr auto operator==(result_t const&) const noexcept -> bool  = default;
    };

    auto operator()(histogram_t const& histogram) const noexcept -> result_t
    {
        if (histogram.count() == 0) return {};

        auto result = result_t{};

        auto const total = histogram.count();
        auto const limit = [total](value_t percentage) noexcept { return (total * percentage + 99) / 100; };
        struct thresholds_t
        {
            value_t  limit;
            value_t& dst;
        };
        thresholds_t const thresholds[] = {
            {limit(50), result.p50}, {limit(90), result.p90}, {limit(95), result.p95},
            {limit(99), result.p99}, {total, result.p100},
        };

        auto       running_sum = value_t{0};
        auto       index       = 0;
        auto const max_index   = std::ssize(thresholds);
        histogram.visit([&](value_t value, int_t count) noexcept {
            running_sum += count;

            // assign crossed thresholds
            while (index < max_index && running_sum >= thresholds[index].limit) thresholds[index++].dst = value;

            return index < max_index;
        });

        return result;
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Distribution
// --------------------------------------------------------------------------------------------------------------------

template <typename value_t, typename histogram_t = histogram_t<value_t>,
          typename percentile_calculator_t = percentile_calculator_t<value_t>>
class distribution_t
{
public:
    distribution_t() = default;

    distribution_t(percentile_calculator_t calc_percentiles, histogram_t histogram) noexcept
        : calc_percentiles_{std::move(calc_percentiles)}, histogram_{std::move(histogram)}
    {}

    auto calc_percentiles() const noexcept -> percentile_calculator_t::result_t
    {
        return calc_percentiles_(histogram_);
    }

    auto sample(value_t value) noexcept -> void { histogram_.sample(value); }

    friend auto operator<<(std::ostream& out, distribution_t const& src) -> std::ostream&
    {
        return out << src.calc_percentiles();
    }

    constexpr auto operator<=>(distribution_t const&) const noexcept -> auto = default;
    constexpr auto operator==(distribution_t const&) const noexcept -> bool  = default;

private:
    [[no_unique_address]] percentile_calculator_t calc_percentiles_{};
    histogram_t                                   histogram_{};
};

// --------------------------------------------------------------------------------------------------------------------
// Stats Accumulator
// --------------------------------------------------------------------------------------------------------------------

/// accumulates stats to provide a summary
template <typename arg_t, typename t_value_t, typename accumulator_t = compensated_accumulator_t<t_value_t>,
          typename arg_min_max_t = arg_min_max_t<arg_t, t_value_t>>
struct stats_accumulator_t
{
    using value_t = t_value_t;

    accumulator_t sse{};
    accumulator_t sum{};
    arg_min_max_t arg_min_max{};
    int_t         sample_count{};

    constexpr auto sample(arg_t arg, value_t error) noexcept -> void
    {
        ++sample_count;

        sse += error * error;
        sum += error;
        arg_min_max.sample(arg, error);
    }

    constexpr auto mse() const noexcept -> value_t
    {
        return sample_count ? sse / static_cast<value_t>(sample_count) : 0;
    }

    constexpr auto rmse() const noexcept -> value_t
    {
        using std::sqrt;
        return sqrt(mse());
    }

    constexpr auto bias() const noexcept -> value_t
    {
        return sample_count ? sum / static_cast<value_t>(sample_count) : 0;
    }

    constexpr auto variance() const noexcept -> value_t
    {
        auto const bias = this->bias();
        return mse() - bias * bias;
    }

    friend auto operator<<(std::ostream& out, stats_accumulator_t const& src) -> std::ostream&
    {
        out << "sample count = " << src.sample_count;
        if (src.sample_count)
        {
            out << "\n"
                << src.arg_min_max << "\n"
                << "sum = " << src.sum << "\nmse = " << src.mse() << "\nrmse = " << src.rmse()
                << "\nbias = " << src.bias() << "\nvariance = " << src.variance();
        }
        return out;
    }

    constexpr auto operator<=>(stats_accumulator_t const&) const noexcept -> auto = default;
    constexpr auto operator==(stats_accumulator_t const&) const noexcept -> bool  = default;
};

} // namespace crv
