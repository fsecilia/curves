// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "one_euro.hpp"

#include <crv/math/abs.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <numbers>

namespace crv::pipeline::filters::one_euro {
namespace {

template <typename t_real_t> struct reference_parameters_t
{
    using real_t = t_real_t;

    real_t derivative_cutoff_rate;
    real_t minimum_cutoff_rate;
    real_t cutoff_slope;
};

/// Independent floating implementation of the reference equations.
///
/// Production intentionally does not share these equations or helpers.
template <typename t_real_t> class reference_filter_t
{
public:
    using real_t = t_real_t;
    using params_t = reference_parameters_t<real_t>;

    constexpr explicit reference_filter_t(params_t params) noexcept : params_{params} {}

    constexpr auto operator()(real_t input, real_t dt_ns) noexcept -> real_t
    {
        assert(input >= real_t{});
        assert(dt_ns > real_t{});

        if (!initialized_)
        {
            filtered_derivative_ = real_t{};
            filtered_signal_ = input;
            initialized_ = true;
            return input;
        }

        auto const raw_derivative = (input - filtered_signal_) / dt_ns;
        auto const derivative_alpha = alpha(params_.derivative_cutoff_rate, dt_ns);
        filtered_derivative_ = filtered_derivative_ + derivative_alpha * (raw_derivative - filtered_derivative_);

        auto const cutoff_rate = params_.minimum_cutoff_rate + params_.cutoff_slope * abs(filtered_derivative_);
        auto const signal_alpha = alpha(cutoff_rate, dt_ns);
        filtered_signal_ = filtered_signal_ + signal_alpha * (input - filtered_signal_);

        return filtered_signal_;
    }

private:
    static constexpr auto alpha(real_t cutoff_rate, real_t dt_ns) noexcept -> real_t
    {
        auto const interval = cutoff_rate * dt_ns;
        return interval / (real_t{1} + interval);
    }

    params_t params_;

    real_t filtered_derivative_{};
    real_t filtered_signal_{};

    bool initialized_{};
};

template <typename real_t> constexpr auto rel_error(real_t actual, real_t expected) noexcept -> real_t
{
    return abs(actual - expected) / max(abs(expected), real_t{1});
}

struct pipeline_filters_one_euro_filter_integration_test_t : Test
{
    using real_t = long double;

    // Test representations only. Production representations should be justified from the production parameter envelope.
    using x_t = fixed_t<int64_t, 45>;
    using dx_t = x_t;

    using cutoff_rate_t = fixed_t<int64_t, 46>;
    using cutoff_slope_t = fixed_t<int64_t, 47>;
    using cutoff_interval_value_t = fixed_t<int64_t, 48>;
    using cutoff_interval_t = cutoff_interval_t<cutoff_interval_value_t>;
    using cutoff_interval_calculator_t = cutoff_interval_calculator_t<cutoff_interval_t>;
    using cutoff_rate_calculator_t = cutoff_rate_calculator_t<cutoff_rate_t>;

    using dt_ns_t = fixed_t<uint64_t, 0>;

    using params_t = parameters_t<cutoff_rate_t, cutoff_slope_t>;

    using derivative_filter_t = derivative_filter_t<x_t, dx_t, cutoff_rate_t, cutoff_interval_calculator_t>;
    using signal_filter_t = signal_filter_t<x_t, cutoff_rate_t, cutoff_interval_calculator_t>;

    using sut_t = filter_t<x_t, dx_t, params_t, derivative_filter_t, cutoff_rate_calculator_t, signal_filter_t>;

    struct sample_t
    {
        int64_t input;
        uint64_t dt_ns;
    };

    static constexpr auto pi = std::numbers::pi_v<real_t>;
    static constexpr auto per_second_to_per_ns = real_t{1e-9L};

    static auto make_params(real_t derivative_cutoff_hz, real_t minimum_cutoff_hz, real_t beta) -> params_t
    {
        auto const derivative_cutoff_rate = real_t{2} * pi * derivative_cutoff_hz * per_second_to_per_ns;
        auto const minimum_cutoff_rate = real_t{2} * pi * minimum_cutoff_hz * per_second_to_per_ns;
        auto const cutoff_slope = real_t{2} * pi * beta;

        return {
            .derivative_cutoff_rate = to_fixed<cutoff_rate_t>(derivative_cutoff_rate),

            .minimum_cutoff_rate = to_fixed<cutoff_rate_t>(minimum_cutoff_rate),

            .cutoff_slope = to_fixed<cutoff_slope_t>(cutoff_slope),
        };
    }

    template <typename t_real_t> static auto make_reference_params(params_t params) -> reference_parameters_t<t_real_t>
    {
        using oracle_t = t_real_t;

        return {
            .derivative_cutoff_rate = from_fixed<oracle_t>(params.derivative_cutoff_rate),
            .minimum_cutoff_rate = from_fixed<oracle_t>(params.minimum_cutoff_rate),
            .cutoff_slope = from_fixed<oracle_t>(params.cutoff_slope),
        };
    }

    static auto ordinary_params() -> params_t { return make_params(real_t{10}, real_t{1}, real_t{0.01L}); }

    template <typename t_real_t, std::size_t n>
    static void expect_matches_reference(
        params_t params, std::array<sample_t, n> const& samples, t_real_t maximum_relative_error)
    {
        using oracle_t = t_real_t;

        ASSERT_TRUE(params.template validate<dx_t>());

        auto sut = sut_t{params};
        auto reference = reference_filter_t<oracle_t>{make_reference_params<oracle_t>(params)};

        for (auto i = std::size_t{}; i < samples.size(); ++i)
        {
            auto const input = x_t{samples[i].input};
            auto const dt_ns = dt_ns_t{samples[i].dt_ns};

            auto const expected = reference(from_fixed<oracle_t>(input), from_fixed<oracle_t>(dt_ns));

            auto const actual = sut(input, dt_ns);
            auto const actual_real = from_fixed<oracle_t>(actual);

            auto const error = rel_error(actual_real, expected);

            EXPECT_LE(error, maximum_relative_error)
                << "sample = " << i << ", input = " << samples[i].input << ", dt_ns = " << samples[i].dt_ns
                << ", expected = " << static_cast<long double>(expected)
                << ", actual = " << static_cast<long double>(actual_real)
                << ", relative error = " << static_cast<long double>(error);
        }
    }
};

TEST_F(pipeline_filters_one_euro_filter_integration_test_t, valid_parameters_validate)
{
    EXPECT_TRUE(ordinary_params().template validate<dx_t>());
}

TEST_F(pipeline_filters_one_euro_filter_integration_test_t, derivative_cutoff_rate_must_be_positive)
{
    auto params = ordinary_params();
    params.derivative_cutoff_rate = cutoff_rate_t{};

    auto const result = params.template validate<dx_t>();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(params_t::validation_error::derivative_cutoff_rate_not_positive, result.error());
}

TEST_F(pipeline_filters_one_euro_filter_integration_test_t, minimum_cutoff_rate_must_be_positive)
{
    auto params = ordinary_params();
    params.minimum_cutoff_rate = cutoff_rate_t{};

    auto const result = params.template validate<dx_t>();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(params_t::validation_error::minimum_cutoff_rate_not_positive, result.error());
}

TEST_F(pipeline_filters_one_euro_filter_integration_test_t, cutoff_slope_must_not_be_negative)
{
    auto params = ordinary_params();
    params.cutoff_slope = cutoff_slope_t{-1};

    auto const result = params.template validate<dx_t>();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(params_t::validation_error::cutoff_slope_negative, result.error());
}

TEST_F(pipeline_filters_one_euro_filter_integration_test_t, first_sample_is_unfiltered)
{
    auto sut = sut_t{ordinary_params()};

    constexpr auto input = x_t{1234};
    constexpr auto dt_ns = dt_ns_t{250'037};

    EXPECT_EQ(input, sut(input, dt_ns));
}

TEST_F(pipeline_filters_one_euro_filter_integration_test_t, constant_input_is_exact_across_arbitrary_intervals)
{
    auto sut = sut_t{ordinary_params()};

    constexpr auto input = x_t{5000};

    ASSERT_EQ(input, sut(input, dt_ns_t{250'013}));

    constexpr auto intervals = std::array<uint64_t, 10>{
        247'811,
        252'441,
        500'017,
        1'000'003,
        125'119,
        3'750'029,
        249'997,
        100'000'123,
        251'071,
        2'000'033,
    };

    for (auto const dt_ns : intervals) { EXPECT_EQ(input, sut(input, dt_ns_t{dt_ns})); }
}

TEST_F(pipeline_filters_one_euro_filter_integration_test_t, variable_intervals_match_reference)
{
    constexpr auto samples = std::array{
        sample_t{0, 250'013},
        sample_t{25, 247'811},
        sample_t{110, 252'441},
        sample_t{360, 500'017},
        sample_t{900, 125'119},
        sample_t{1700, 375'007},
        sample_t{3200, 249'997},
        sample_t{5000, 251'071},
        sample_t{6200, 1'750'019},
        sample_t{6600, 250'103},
        sample_t{5400, 250'017},
        sample_t{3000, 749'999},
        sample_t{800, 250'029},
        sample_t{0, 100'000'123},
    };

    // Integration-level behavioral tolerance only. Component tests should
    // later use an error budget expressed in fixed-point output quanta.
    constexpr auto tolerance = real_t{1e-6L};

    expect_matches_reference<real_t>(ordinary_params(), samples, tolerance);
}

TEST_F(pipeline_filters_one_euro_filter_integration_test_t, zero_beta_matches_reference)
{
    auto params = ordinary_params();
    params.cutoff_slope = cutoff_slope_t{};

    constexpr auto samples = std::array{
        sample_t{0, 250'003},
        sample_t{1000, 249'019},
        sample_t{1000, 251'007},
        sample_t{1000, 500'011},
        sample_t{1000, 2'000'017},
        sample_t{1000, 137'003},
        sample_t{1000, 100'000'019},
    };

    constexpr auto tolerance = real_t{1e-6L};

    expect_matches_reference<real_t>(params, samples, tolerance);
}

TEST_F(pipeline_filters_one_euro_filter_integration_test_t, coupled_interval_jitter_matches_reference)
{
    constexpr auto samples = std::array{
        sample_t{1000, 250'000},
        sample_t{1500, 150'000},
        sample_t{1800, 350'000},
        sample_t{2600, 147'000},
        sample_t{3000, 353'000},
        sample_t{4200, 152'000},
        sample_t{4600, 348'000},
        sample_t{5800, 149'000},
        sample_t{6100, 351'000},
        sample_t{7000, 250'000},
    };

    constexpr auto tolerance = real_t{1e-6L};

    expect_matches_reference<real_t>(ordinary_params(), samples, tolerance);
}

TEST_F(pipeline_filters_one_euro_filter_integration_test_t, long_positive_interval_matches_reference_without_reset)
{
    constexpr auto samples = std::array{
        sample_t{1000, 250'000},
        sample_t{1500, 250'000},
        sample_t{2000, 250'000},

        // Ordinary positive elapsed time, not a reset boundary.
        sample_t{2500, 500'000'123},

        sample_t{6000, 249'997},
        sample_t{9000, 250'013},
    };

    constexpr auto tolerance = real_t{1e-6L};

    expect_matches_reference<real_t>(ordinary_params(), samples, tolerance);
}

} // namespace
} // namespace crv::pipeline::filters::one_euro
