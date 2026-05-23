// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "span_decomposer.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <utility>
#include <vector>

namespace crv::spline::seed {
namespace {

struct spline_seed_span_decomposer_test_t : Test
{
    using signed_t = int_t;
    using unsigned_t = uint_t;
    using fixed_t = fixed_t<signed_t, 3>;
    using x_t = fixed_t;

    using function_sample_t = x_t;

    // this is a callable in prod, but it is not called through here, so we just need the id
    struct sample_target_function_t
    {
        int_t id = 0;
        auto operator==(sample_target_function_t const&) const noexcept -> bool = default;
    };

    struct subdomain_t
    {
        sample_target_function_t sample_target_function;
        function_sample_t left_sample;
        x_t left;
        x_t stride;
        function_sample_t right = left + stride;
        auto operator==(subdomain_t const&) const noexcept -> bool = default;
    };

    struct interval_t
    {
        subdomain_t subdomain;
        auto operator==(interval_t const&) const noexcept -> bool = default;
    };

    using intervals_t = std::vector<interval_t>;
    struct refinement_pool_t
    {
        intervals_t intervals;
        constexpr auto emplace(auto&& interval) -> void
        {
            intervals.emplace_back(std::forward<decltype(interval)>(interval));
        }
        constexpr auto size() const noexcept -> std::size_t { return intervals.size(); }
    };

    static constexpr auto max_segment_count = 4;
    static constexpr auto log2_min_width = -1;
    static constexpr auto min_segment_width = x_t{1} >> -log2_min_width;
    static constexpr auto epsilon = x_t::literal(1);

    sample_target_function_t sample_target_function{.id = 1};
    refinement_pool_t refinement_pool{};
};

//
// mocking tests
//

struct spline_seed_span_decomposer_mocking_test_t : spline_seed_span_decomposer_test_t
{
    struct mock_stride_calculator_t
    {
        virtual ~mock_stride_calculator_t() = default;

        MOCK_METHOD(x_t, call, (x_t const& left, x_t const& right), (const, noexcept));
    };
    StrictMock<mock_stride_calculator_t> mock_stride_calculator;

    struct stride_calculator_t
    {
        mock_stride_calculator_t* mock = nullptr;

        auto operator()(x_t const& left, x_t const& right) const noexcept -> x_t { return mock->call(left, right); }
    };

    struct mock_subdomain_factory_t
    {
        virtual ~mock_subdomain_factory_t() = default;

        MOCK_METHOD(subdomain_t, call,
            (sample_target_function_t const& sample_target_function, function_sample_t const& left_sample,
                x_t const& left, x_t const& stride),
            (const, noexcept));
    };
    StrictMock<mock_subdomain_factory_t> mock_subdomain_factory;

    struct subdomain_factory_t
    {
        using x_t = x_t;

        struct scalar_t
        {};

        struct jet_t
        {};

        using function_sample_t = function_sample_t;
        using unsigned_t = unsigned_t;

        mock_subdomain_factory_t* mock = nullptr;

        auto operator()(sample_target_function_t const& sample_target_function, function_sample_t const& left_sample,
            x_t const& left, x_t const& stride) const noexcept -> subdomain_t
        {
            return mock->call(sample_target_function, left_sample, left, stride);
        }
    };

    struct mock_interval_factory_t
    {
        virtual ~mock_interval_factory_t() = default;

        MOCK_METHOD(interval_t, call,
            (sample_target_function_t const& sample_target_function, subdomain_t const& subdomain), (const, noexcept));
    };
    StrictMock<mock_interval_factory_t> mock_interval_factory;

    struct interval_factory_t
    {
        mock_interval_factory_t* mock = nullptr;

        auto operator()(sample_target_function_t const& sample_target_function,
            subdomain_t const& subdomain) const noexcept -> interval_t
        {
            return mock->call(sample_target_function, subdomain);
        }
    };

    using sut_t = span_decomposer_t<stride_calculator_t, subdomain_factory_t, interval_factory_t, max_segment_count,
        log2_min_width>;

    sut_t sut{
        .calculate_stride = stride_calculator_t{&mock_stride_calculator},
        .create_subdomain = subdomain_factory_t{&mock_subdomain_factory},
        .create_interval = interval_factory_t{&mock_interval_factory},
    };

    auto expect_iteration(subdomain_t subdomain, x_t right) noexcept -> void
    {
        EXPECT_CALL(mock_stride_calculator, call(subdomain.left, right)).WillOnce(Return(subdomain.stride));
        EXPECT_CALL(mock_subdomain_factory,
            call(sample_target_function, subdomain.left_sample, subdomain.left, subdomain.stride))
            .WillOnce(Return(subdomain));
        EXPECT_CALL(mock_interval_factory, call(sample_target_function, subdomain))
            .WillOnce(Return(interval_t{.subdomain = subdomain}));
    }

    auto create_subdomain(x_t left, x_t stride) const noexcept -> subdomain_t
    {
        return subdomain_t{
            .sample_target_function = sample_target_function,
            .left_sample = left,
            .left = left,
            .stride = stride,
        };
    }
};

TEST_F(spline_seed_span_decomposer_mocking_test_t, empty_span)
{
    auto const left = x_t{16};
    auto const right = left;
    function_sample_t left_sample = left;

    auto const result = sut(sample_target_function, left_sample, left, right, refinement_pool);

    EXPECT_EQ(result, right);

    EXPECT_TRUE(refinement_pool.intervals.empty());
}

TEST_F(spline_seed_span_decomposer_mocking_test_t, single_stride)
{
    auto const left = x_t{16};
    auto const right = x_t{32};
    function_sample_t left_sample = left;
    auto const stride = right - left;

    auto const subdomain = create_subdomain(left, stride);

    expect_iteration(subdomain, right);

    auto const result = sut(sample_target_function, left_sample, left, right, refinement_pool);
    EXPECT_EQ(result, right);

    auto const expected_intervals = intervals_t{{.subdomain = subdomain}};
    EXPECT_EQ(refinement_pool.intervals, expected_intervals);
}

TEST_F(spline_seed_span_decomposer_mocking_test_t, single_stride_min_segment_width)
{
    auto const left = x_t{16};
    auto const right = left + min_segment_width;
    function_sample_t left_sample = left;
    auto const stride = right - left;

    auto const subdomain = create_subdomain(left, stride);

    expect_iteration(subdomain, right);

    auto const result = sut(sample_target_function, left_sample, left, right, refinement_pool);
    EXPECT_EQ(result, right);

    auto const expected_intervals = intervals_t{{.subdomain = subdomain}};
    EXPECT_EQ(refinement_pool.intervals, expected_intervals);
}

TEST_F(spline_seed_span_decomposer_mocking_test_t, two_strides_same_size_aligned)
{
    auto const left = x_t{16};
    auto const right = x_t{32};
    function_sample_t left_sample = left;
    auto const stride = (right - left) / 2;

    subdomain_t const subdomains[] = {
        create_subdomain(left, stride),
        create_subdomain(left + stride, stride),
    };

    auto const seq = InSequence{};

    expect_iteration(subdomains[0], right);
    expect_iteration(subdomains[1], right);

    auto const result = sut(sample_target_function, left_sample, left, right, refinement_pool);
    EXPECT_EQ(result, right);

    auto const expected_intervals = intervals_t{
        {.subdomain = subdomains[0]},
        {.subdomain = subdomains[1]},
    };
    EXPECT_EQ(refinement_pool.intervals, expected_intervals);
}

TEST_F(spline_seed_span_decomposer_mocking_test_t, two_strides_different_size_aligned)
{
    auto const left = x_t{16};
    auto const right = x_t{64};
    function_sample_t left_sample = left;

    subdomain_t const subdomains[] = {
        create_subdomain(left, x_t{16}),
        create_subdomain(left + 16, x_t{32}),
    };

    auto const seq = InSequence{};

    expect_iteration(subdomains[0], right);
    expect_iteration(subdomains[1], right);

    auto const result = sut(sample_target_function, left_sample, left, right, refinement_pool);
    EXPECT_EQ(result, right);

    auto const expected_intervals = intervals_t{
        {.subdomain = subdomains[0]},
        {.subdomain = subdomains[1]},
    };
    EXPECT_EQ(refinement_pool.intervals, expected_intervals);
}

TEST_F(spline_seed_span_decomposer_mocking_test_t, two_strides_different_size_start_unaligned)
{
    auto const left = x_t{20};
    auto const right = x_t{32};
    function_sample_t left_sample = left;

    subdomain_t const subdomains[] = {
        create_subdomain(left, x_t{4}),
        create_subdomain(left + 4, x_t{8}),
    };

    auto const seq = InSequence{};

    expect_iteration(subdomains[0], right);
    expect_iteration(subdomains[1], right);

    auto const result = sut(sample_target_function, left_sample, left, right, refinement_pool);
    EXPECT_EQ(result, right);

    auto const expected_intervals = intervals_t{
        {.subdomain = subdomains[0]},
        {.subdomain = subdomains[1]},
    };
    EXPECT_EQ(refinement_pool.intervals, expected_intervals);
}

TEST_F(spline_seed_span_decomposer_mocking_test_t, fill_budget_stride_grows_shrinks)
{
    auto const left = x_t{20};
    auto const right = x_t{41}; // 20 + 4 + 8 + 8 + 1
    function_sample_t left_sample = left;

    subdomain_t const subdomains[] = {
        create_subdomain(x_t{20}, x_t{4}),
        create_subdomain(x_t{24}, x_t{8}),
        create_subdomain(x_t{32}, x_t{8}),
        create_subdomain(x_t{40}, x_t{1}),
    };

    auto const seq = InSequence{};

    expect_iteration(subdomains[0], right);
    expect_iteration(subdomains[1], right);
    expect_iteration(subdomains[2], right);
    expect_iteration(subdomains[3], right);

    auto const result = sut(sample_target_function, left_sample, left, right, refinement_pool);
    EXPECT_EQ(result, right);

    auto const expected_intervals = intervals_t{
        {.subdomain = subdomains[0]},
        {.subdomain = subdomains[1]},
        {.subdomain = subdomains[2]},
        {.subdomain = subdomains[3]},
    };
    EXPECT_EQ(refinement_pool.intervals, expected_intervals);
}

TEST_F(spline_seed_span_decomposer_mocking_test_t, left_sample_threaded_from_subdomain_right)
{
    auto const left = x_t{16};
    auto const right = x_t{32};
    function_sample_t left_sample = left;
    auto const stride = (right - left) / 2;

    // first subdomain's .right is deliberately unrelated to position arithmetic, so the only way the second iteration
    // can receive it is through the threading assignment: left_sample = subdomain.right
    auto const first_subdomain = subdomain_t{
        .sample_target_function = sample_target_function,
        .left_sample = left,
        .left = left,
        .stride = stride,
        .right = x_t{99},
    };

    auto const second_subdomain = subdomain_t{
        .sample_target_function = sample_target_function,
        .left_sample = first_subdomain.right,
        .left = left + stride,
        .stride = stride,
        .right = x_t{37},
    };

    auto const seq = InSequence{};

    // first iteration: receives the original left_sample
    EXPECT_CALL(mock_stride_calculator, call(left, right)).WillOnce(Return(stride));
    EXPECT_CALL(mock_subdomain_factory, call(sample_target_function, left_sample, left, stride))
        .WillOnce(Return(first_subdomain));
    EXPECT_CALL(mock_interval_factory, call(sample_target_function, first_subdomain))
        .WillOnce(Return(interval_t{.subdomain = first_subdomain}));

    // second iteration: must receive x_t{99} as left_sample
    EXPECT_CALL(mock_stride_calculator, call(left + stride, right)).WillOnce(Return(stride));
    EXPECT_CALL(mock_subdomain_factory, call(sample_target_function, function_sample_t{x_t{99}}, left + stride, stride))
        .WillOnce(Return(second_subdomain));
    EXPECT_CALL(mock_interval_factory, call(sample_target_function, second_subdomain))
        .WillOnce(Return(interval_t{.subdomain = second_subdomain}));

    auto const result = sut(sample_target_function, left_sample, left, right, refinement_pool);

    // return value is the last subdomain.right
    EXPECT_EQ(result, second_subdomain.right);

    auto const expected_intervals = intervals_t{
        {.subdomain = first_subdomain},
        {.subdomain = second_subdomain},
    };
    EXPECT_EQ(refinement_pool.intervals, expected_intervals);
}

//
// death tests
//

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct spline_seed_span_decomposer_death_test_t : spline_seed_span_decomposer_test_t
{
    x_t stride_calculator_result{0};

    struct stride_calculator_t
    {
        x_t* result = nullptr;
        auto operator()(x_t const&, x_t const&) const noexcept -> x_t { return *result; }
    };

    struct subdomain_factory_t
    {
        using x_t = x_t;

        struct scalar_t
        {};

        struct jet_t
        {};

        using function_sample_t = function_sample_t;
        using unsigned_t = unsigned_t;

        auto operator()(sample_target_function_t const&, function_sample_t const&, x_t const&,
            x_t const&) const noexcept -> subdomain_t
        {
            return {};
        }
    };

    struct interval_factory_t
    {
        auto operator()(sample_target_function_t const&, subdomain_t const&) const noexcept -> interval_t { return {}; }
    };

    using sut_t = span_decomposer_t<stride_calculator_t, subdomain_factory_t, interval_factory_t, max_segment_count,
        log2_min_width>;

    sut_t sut{
        .calculate_stride = stride_calculator_t{&stride_calculator_result},
        .create_subdomain = subdomain_factory_t{},
        .create_interval = interval_factory_t{},
    };
};

TEST_F(spline_seed_span_decomposer_death_test_t, not_monotonic)
{
    auto const left = x_t{20};
    auto const right = x_t{18};
    function_sample_t left_sample = left;

    EXPECT_DEBUG_DEATH(
        sut(sample_target_function, left_sample, left, right, refinement_pool), "monotonically increasing");
}

TEST_F(spline_seed_span_decomposer_death_test_t, right_misaligned)
{
    auto const left = x_t{16};
    auto const right = x_t{32} + epsilon;
    function_sample_t left_sample = left;

    EXPECT_DEBUG_DEATH(sut(sample_target_function, left_sample, left, right, refinement_pool), "not aligned");
}

TEST_F(spline_seed_span_decomposer_death_test_t, stride_exceeds_right_boundary)
{
    auto const left = x_t{16};
    auto const right = x_t{32};
    function_sample_t left_sample = left;

    stride_calculator_result = (right - left) * 2;

    EXPECT_DEBUG_DEATH(sut(sample_target_function, left_sample, left, right, refinement_pool),
        "stride must not exceed right boundary");
}

TEST_F(spline_seed_span_decomposer_death_test_t, budget_exceeded)
{
    auto const left = x_t{20};
    auto const right = x_t{43};
    function_sample_t left_sample = left;

    std::fill_n(std::back_inserter(refinement_pool.intervals), max_segment_count, interval_t{});

    EXPECT_DEBUG_DEATH(
        sut(sample_target_function, left_sample, left, right, refinement_pool), "exceeded segment budget");
}

TEST_F(spline_seed_span_decomposer_death_test_t, stride_smaller_than_min_segment_width)
{
    auto const left = x_t{16};
    auto const right = x_t{32};
    function_sample_t left_sample = left;

    stride_calculator_result = min_segment_width / 2;

    EXPECT_DEBUG_DEATH(sut(sample_target_function, left_sample, left, right, refinement_pool), "min segment width");
}

TEST_F(spline_seed_span_decomposer_death_test_t, stride_must_be_dyadic)
{
    auto const left = x_t{16};
    auto const right = x_t{32};
    function_sample_t left_sample = left;

    stride_calculator_result = min_segment_width - epsilon;

    EXPECT_DEBUG_DEATH(sut(sample_target_function, left_sample, left, right, refinement_pool), "stride must be dyadic");
}

TEST_F(spline_seed_span_decomposer_death_test_t, smallest_representable_midpoint)
{
    auto const left = x_t{16};
    auto const right = x_t{32};
    function_sample_t left_sample = left;

    stride_calculator_result = epsilon * 2;

    EXPECT_DEBUG_DEATH(sut(sample_target_function, left_sample, left, right, refinement_pool), "min segment width");
}

TEST_F(spline_seed_span_decomposer_death_test_t, unrepresentable_midpoint)
{
    auto const left = x_t{16};
    auto const right = x_t{32};
    function_sample_t left_sample = left;

    stride_calculator_result = epsilon;

    EXPECT_DEBUG_DEATH(
        sut(sample_target_function, left_sample, left, right, refinement_pool), "midpoint.*representable");
}

#endif

} // namespace
} // namespace crv::spline::seed
