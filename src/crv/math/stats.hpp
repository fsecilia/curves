// SPDX-License-Identifier: MIT
/**
    \file
    \brief basic statisticvs

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <concepts>
#include <ostream>
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
    using values_t = std::vector<int_t>;

    histogram_t(values_t negative, values_t positive) noexcept
        : negative_{std::move(negative)}, positive_{std::move(positive)}
    {
        assert((negative_.empty() || negative_[0] == 0) && "histogram_t: negative_[0] is always skipped");

        // count elements
        for (auto const element : negative_) count_ += element;
        for (auto const element : positive_) count_ += element;

        // strip trailing zeros
        while (!negative_.empty() && !negative_.back()) negative_.pop_back();
        while (!positive_.empty() && !positive_.back()) positive_.pop_back();
    }

    histogram_t() = default;

    auto count() const noexcept -> int_t { return count_; }

    auto sample(value_t value) -> void
    {
        if (value >= 0) inc(positive_, value);
        else inc(negative_, -value);
        ++count_;
    }

    template <typename visitor_t> auto visit(visitor_t&& visitor) const -> void
    {
        // walk negative in reverse order, skipping [0]
        for (auto i = std::ssize(negative_) - 1; i > 0; --i)
        {
            if (negative_[i] > 0)
            {
                if (!visitor(static_cast<value_t>(-i), negative_[i])) return;
            }
        }

        // walk positive in forward order
        for (auto i = 0; i < std::ssize(positive_); ++i)
        {
            if (positive_[i] > 0)
            {
                if (!visitor(static_cast<value_t>(i), positive_[i])) return;
            }
        }
    }

    constexpr auto operator<=>(histogram_t const&) const noexcept -> auto = default;
    constexpr auto operator==(histogram_t const&) const noexcept -> bool  = default;

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

private:
    int_t    count_{};
    values_t negative_{};
    values_t positive_{};

    auto inc(values_t& values, value_t value) noexcept -> void
    {
        if (std::cmp_greater_equal(value, std::size(values))) values.resize(value + 1);
        ++values[value];
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Percentiles
// --------------------------------------------------------------------------------------------------------------------

struct percentile_calculator_t
{
    struct result_t
    {
        int_t p50{};
        int_t p90{};
        int_t p95{};
        int_t p99{};
        int_t p100{};

        constexpr auto operator<=>(result_t const&) const noexcept -> auto = default;
        constexpr auto operator==(result_t const&) const noexcept -> bool  = default;

        friend auto operator<<(std::ostream& out, result_t const& src) -> std::ostream&
        {
            return out << "p50 = " << src.p50 << ", p90 = " << src.p90 << ", p95 = " << src.p95 << ", p99 = " << src.p99
                       << ", max = " << src.p100;
        }
    };

    auto operator()(auto const& histogram) const noexcept -> result_t
    {
        if (histogram.count() == 0) return {};

        auto result = result_t{};

        auto const total = histogram.count();
        auto const limit = [total](int_t percentage) noexcept { return (total * percentage + 99) / 100; };
        struct thresholds_t
        {
            int_t  limit;
            int_t& dst;
        };
        thresholds_t const thresholds[] = {
            {limit(50), result.p50}, {limit(90), result.p90}, {limit(95), result.p95},
            {limit(99), result.p99}, {total, result.p100},
        };

        auto       running_sum = 0;
        auto       index       = 0;
        auto const max_index   = static_cast<int_t>(std::size(thresholds));
        histogram.visit([&](auto value, auto count) noexcept {
            running_sum += count;

            // assign crossed thresholds
            while (index < max_index && running_sum >= thresholds[index].limit) thresholds[index++].dst = value;

            return index < max_index;
        });

        return result;
    }
};

} // namespace crv
