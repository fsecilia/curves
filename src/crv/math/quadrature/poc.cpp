// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/quadrature/rules.hpp>
#include <crv/ranges.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <concepts>
#include <flat_map>
#include <numeric>

namespace crv::quadrature {
namespace {

using rule_t = rules::gauss_kronrod_t<15>;

template <std::floating_point t_real_t> struct segment_t
{
    using real_t = t_real_t;

    real_t left;
    real_t right;
    real_t integral;
    real_t tolerance;
    int_t  depth;
};

template <std::floating_point t_real_t> struct bisection_t
{
    using real_t    = t_real_t;
    using segment_t = segment_t<real_t>;

    segment_t left;
    segment_t right;
    real_t    integral;
    real_t    error_estimate;
};

template <typename integrand_t, typename rule_t> class integral_t
{
public:
    constexpr integral_t(integrand_t integrand, rule_t rule) noexcept
        : integrand_{std::move(integrand)}, rule_{std::move(rule)}
    {}

    /// integrates over [left, right]
    template <std::floating_point real_t>
    constexpr auto operator()(real_t left, real_t right) const noexcept -> rule_result_t<real_t>
    {
        return rule_(left, right, integrand_);
    }

    /// evaluates integrand at a point
    template <std::floating_point real_t> constexpr auto operator()(real_t position) const noexcept -> real_t
    {
        return integrand_(position);
    }

private:
    integrand_t integrand_;
    rule_t      rule_;
};

template <typename integral_t> class quadrature_t
{
public:
    constexpr quadrature_t(integral_t integral) noexcept : integral_{std::move(integral)} {}

    /// creates a root segment, integrating over [left, right]
    template <std::floating_point real_t>
    constexpr auto operator()(real_t left, real_t right, real_t tolerance) const noexcept -> segment_t<real_t>
    {
        return segment_t{left, right, integral_(left, right).value, tolerance, 0};
    }

    /// bisects segment
    template <std::floating_point real_t>
    constexpr auto operator()(segment_t<real_t> const& parent) const noexcept -> bisection_t<real_t>
    {
        auto const parent_midpoint = std::midpoint(parent.left, parent.right);
        auto const child_tolerance = parent.tolerance / 2;
        auto const child_depth     = parent.depth + 1;

        auto const left_rule_result  = integral_(parent.left, parent_midpoint);
        auto const right_rule_result = integral_(parent_midpoint, parent.right);

        auto const combined_integral = left_rule_result.value + right_rule_result.value;
        auto const quadrature_error  = left_rule_result.error + right_rule_result.error;
        auto const subdivision_error = abs(combined_integral - parent.integral);
        auto const error_estimate    = std::max(quadrature_error, subdivision_error);

        // clang-format off
        return bisection_t
        {
            .left = segment_t
            {
                .left = parent.left,
                .right = parent_midpoint,
                .integral = left_rule_result.value,
                .tolerance = child_tolerance,
                .depth = child_depth,
            },
            .right = segment_t
            {
                .left = parent_midpoint,
                .right = parent.right,
                .integral = right_rule_result.value,
                .tolerance = child_tolerance,
                .depth = child_depth,
            },
            .integral = combined_integral,
            .error_estimate = error_estimate,
        };
        // clang-format on
    }

    constexpr auto finalize() && -> integral_t { return std::move(integral_); }

private:
    integral_t integral_{};
};

/// working stack of segments in strictly left-to-right order
template <typename segment_t, typename segments_t = std::vector<segment_t>> class stack_t
{
public:
    constexpr stack_t() noexcept { segments_.reserve(32); }
    constexpr stack_t(segments_t segments) noexcept : segments_{std::move(segments)} {}

    /// pushes root segment directly
    constexpr auto push(segment_t segment) -> void { segments_.push_back(segment); }

    /// pushes bisected children
    constexpr auto push(auto const& bisection) -> void
    {
        // push right then left so left pops first
        push(bisection.right);
        push(bisection.left);
    }

    /// \pre !empty()
    constexpr auto pop() -> segment_t
    {
        assert(!empty() && "stack_t: pop on empty");
        auto result = segments_.back();
        segments_.pop_back();
        return result;
    }

    constexpr auto clear() noexcept -> void { segments_.clear(); }
    constexpr auto empty() const noexcept -> bool { return segments_.empty(); }

private:
    segments_t segments_{};
};

/// seeds empty stack with domain-level segments
class stack_seeder_t
{
public:
    /// seeds stack with single segment across entire domain
    ///
    /// \pre stack.empty()
    template <std::floating_point real_t>
    constexpr auto seed(auto& stack, auto const& subdivider, real_t domain_max, real_t global_tolerance) -> void
    {
        assert(stack.empty() && "stack_seeder_t: stack must be empty before seeding");

        stack.push(subdivider(real_t{0}, domain_max, global_tolerance));
    }

    /// seeds stack with multiple segments, splitting domain at critical points
    ///
    /// critical_points should not include 0 and domain_max; these are implied
    ///
    /// \pre stack.empty()
    /// \pre critical_points are sorted increasing and unique
    /// \pre critical_points in (0, domain_max)
    template <std::floating_point real_t>
    constexpr auto seed(auto& stack, auto const& subdivider, real_t domain_max, real_t global_tolerance,
                        compatible_range<real_t> auto const& critical_points) -> void
    {
        assert(stack.empty() && "stack_seeder_t: stack must be empty before seeding");

        // push in reverse order so leftmost segment pops first
        auto right = domain_max;
        for (auto const critical_point : critical_points | std::views::reverse)
        {
            auto const left = static_cast<real_t>(critical_point);
            assert((real_t{0} < left && left < domain_max)
                   && "stack_seeder_t: critical points must be in (0, domain_max)");
            assert(left < right && "stack_seeder_t: critical points must be sorted increasing and unique");

            auto const tolerance = global_tolerance * ((right - left) / domain_max);
            stack.push(subdivider(left, right, tolerance));

            right = left;
        }

        stack.push(subdivider(real_t{0}, right, global_tolerance * (right / domain_max)));
    }
};

/// continuous accumulation function F(x), yielding a 1-jet
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

    constexpr auto operator()(real_t location) const noexcept -> jet_t
    {
        assert(intervals_.keys().front() <= location && location <= intervals_.keys().back()
               && "antiderivative_t: domain error");

        auto const right    = intervals_.upper_bound(location);
        auto const left     = std::ranges::prev(right);
        auto const residual = integral_(left->first, location).value;
        auto const integral = left->second + residual;

        return jet_t{integral, integral_(location)};
    }

private:
    integral_t integral_;
    map_t      intervals_;
};

/// accumulates quadrature results and assembles the final antiderivative
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

    constexpr auto finalize(integral_t integral) && -> result_t
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

/// adaptive subdivision loop
class subdivider_t
{
public:
    constexpr auto run(auto& stack, auto const& quadrature, auto& builder, int_t depth_limit) const -> void
    {
        while (!stack.empty())
        {
            auto const segment   = stack.pop();
            auto const bisection = quadrature(segment);

            if (needs_refinement(segment, bisection, depth_limit)) stack.push(bisection);
            else builder.append(bisection.right.right, bisection.integral, bisection.error_estimate);
        }
    }

private:
    template <typename segment_t>
    constexpr auto needs_refinement(segment_t const& segment, auto const& bisection, int_t depth_limit) const noexcept
        -> bool
    {
        using real_t = segment_t::real_t;

        static constexpr auto min_width             = std::numeric_limits<real_t>::epsilon() * real_t{1024};
        static constexpr auto relative_noise_margin = std::numeric_limits<real_t>::epsilon() * real_t{64};

        auto const current_width   = segment.right - segment.left;
        auto const noise_floor     = abs(bisection.integral) * relative_noise_margin;
        auto const local_tolerance = std::max(segment.tolerance, noise_floor);
        return segment.depth < depth_limit && current_width > min_width && bisection.error_estimate > local_tolerance;
    }
};

/// top-level adaptive quadrature engine
template <std::floating_point real_t, typename subdivider_t, typename stack_seeder_t, typename stack_t,
          typename antiderivative_builder_t>
class adaptive_integrator_t
{
public:
    using result_t = antiderivative_builder_t::result_t;

    constexpr adaptive_integrator_t(subdivider_t subdivider, stack_seeder_t stack_seeder, stack_t stack,
                                    real_t tolerance, int_t depth_limit)
        : subdivider_{std::move(subdivider)}, stack_seeder_{std::move(stack_seeder)}, stack_{std::move(stack)},
          tolerance_{tolerance}, depth_limit_{depth_limit}
    {}

    /// wide overload: full DI, every ephemeral injected
    constexpr auto operator()(auto quadrature, antiderivative_builder_t antiderivative_builder, real_t domain_max,
                              compatible_range<real_t> auto const& critical_points) -> result_t
    {
        stack_seeder_.seed(stack_, quadrature, domain_max, tolerance_, critical_points);
        subdivider_.run(stack_, quadrature, antiderivative_builder, depth_limit_);

        return std::move(antiderivative_builder).finalize(std::move(quadrature).finalize());
    }

    /// narrow overload: convenience, constructs ephemerals and delegates to wide
    constexpr auto operator()(auto integral, real_t domain_max, compatible_range<real_t> auto const& critical_points)
    {
        return operator()(quadrature_t{std::move(integral)}, antiderivative_builder_t{}, domain_max, critical_points);
    }

    /// narrow overload: convenience, constructs ephemerals and delegates to wide
    constexpr auto operator()(auto integral, real_t domain_max)
    {
        return operator()(quadrature_t{std::move(integral)}, antiderivative_builder_t{}, domain_max,
                          std::array<real_t, 0>{});
    }

private:
    subdivider_t   subdivider_;
    stack_seeder_t stack_seeder_;
    stack_t        stack_;
    real_t         tolerance_;
    int_t          depth_limit_;
};

constexpr auto create() -> auto
{
    using real_t    = float_t; // float_t is float64_t, double
    using segment_t = segment_t<real_t>;

    auto integral    = integral_t{[](real_t x) static noexcept -> real_t { return x * x; }, rule_t{}};
    using integral_t = decltype(integral);

    using antiderivative_t = antiderivative_t<real_t, integral_t>;
    using antiderivative_builder_t
        = antiderivative_builder_t<real_t, compensated_accumulator_t<real_t>, antiderivative_t>;
    using stack_t = stack_t<segment_t>;
    using adaptive_integrator_t
        = adaptive_integrator_t<real_t, subdivider_t, stack_seeder_t, stack_t, antiderivative_builder_t>;

    auto const tolerance   = 1e-12;
    auto const depth_limit = 64;
    auto const domain_max  = 256;

    auto adaptive_integrator
        = adaptive_integrator_t{subdivider_t{}, stack_seeder_t{}, stack_t{}, tolerance, depth_limit};

    return adaptive_integrator(integral, domain_max, std::array{0.25});
}

TEST(quadrature, poc)
{
    using real_t = float_t; // float_t is float64_t, double

    auto result = create();
    std::cout << "achieved_error = " << result.achieved_error << ", "
              << "max_error = " << result.max_error << std::endl;

    auto const& actual_antiderivative   = result.antiderivative;
    auto        expected_antiderivative = [](real_t x) static noexcept -> real_t { return x * x * x / 3.0; };

    auto const inputs = std::array{0.0, 0.1, 0.25, 0.5, 1.0, 5.0, 10.0, 100.0};
    for (auto const input : inputs)
    {
        auto const actual   = actual_antiderivative(input);
        auto const expected = jet_t<real_t>{expected_antiderivative(input), input * input};
        std::cout << "input: " << input << ", actual = " << actual << ", expected = " << expected << std::endl;
    }
}

} // namespace
} // namespace crv::quadrature
