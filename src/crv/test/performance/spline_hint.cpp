// SPDX-License-Identifier: MIT

/// \file
/// \brief rotating-cache spline locator hint benchmark
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/io/capture/file.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/model/composed_curve.hpp>
#include <crv/model/config.hpp>
#include <crv/pipeline/filters/half_life_ema.hpp>
#include <crv/pipeline/input_frame.hpp>
#include <crv/pipeline/relative_report.hpp>
#include <crv/pipeline/report_timer.hpp>
#include <crv/pipeline/velocity.hpp>
#include <crv/prefetcher.hpp>
#include <crv/serialization/toml/toml.hpp>
#include <crv/spline/construction/curve_target.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/segment_locator.hpp>
#include <crv/spline/spline_factory.hpp>
#include <crv/spline/spline_factory_policy.hpp>
#include <crv/test/performance/performance.hpp>
#include <crv/tuple.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace crv {
namespace {

using x_t = spline::prod_pipeline_config_t::x_t;
constexpr auto depth_max = int_t{4};
using production_locator_t = spline::segment_locator_t<x_t, depth_max>;
using spline_policy_t = spline::default_spline_policy_t<float_t, spline::prod_pipeline_config_t>;
using production_spline_t = spline_policy_t::spline_t;
using spline_factory_t = spline::spline_factory_t<spline_policy_t, spline::spline_generator_factory_t<spline_policy_t>>;

using replay_magnitude_rsqrt_t = rsqrt_t<fixed_t<uint64_t, 62>, fixed_t<uint64_t, 0>>;
using replay_magnitude_t = pipeline::displacement_magnitude_t<replay_magnitude_rsqrt_t>;
using replay_velocity_t = pipeline::velocity_t<x_t, replay_magnitude_t>;
using replay_duration_t = replay_velocity_t::duration_t;
using replay_filter_t = pipeline::half_life_ema_t<x_t, replay_duration_t>;
using replay_timer_t = pipeline::report_timer_t<replay_duration_t>;

static_assert(std::same_as<typename production_spline_t::segment_locator_t, production_locator_t>);

/// benchmark-visible mirror of the production quaternary tree
///
/// The production locator deliberately hides its tree. This mirror keeps candidate hint policies out of the runtime API
/// until measurements settle the design. Startup verification compares every segment against the production locator.
class benchmark_locator_t
{
public:
    static constexpr auto branching_factor = production_locator_t::branching_factor;
    static constexpr auto max_segment_count = production_locator_t::max_segment_count;
    static constexpr auto total_key_count = production_locator_t::total_key_count;
    static constexpr auto node_key_count = production_locator_t::node_key_count;
    static constexpr auto node_count = production_locator_t::node_count;
    static constexpr auto leaf_base_index = ((1 << (2 * (depth_max - 1))) - 1) / node_key_count;
    [[maybe_unused]] static constexpr auto leaf_count = 1 << (2 * (depth_max - 1));

    using node_keys_t = production_locator_t::node_keys_t;
    using node_t = production_locator_t::node_t;
    using nodes_t = production_locator_t::nodes_t;

    struct result_t
    {
        int_t index;
        x_t origin;
    };

    struct extended_result_t
    {
        int_t index;
        x_t origin;
        x_t end;
        int_t leaf_index;
        x_t leaf_origin;
        x_t leaf_end;
    };

    benchmark_locator_t() = default;

    explicit benchmark_locator_t(std::array<x_t, total_key_count> const& sorted_keys, x_t x_max) noexcept
        : x_max_{x_max}
    {
        for (auto in_order_index = 1; in_order_index <= total_key_count; ++in_order_index)
        {
            auto const location = node_location_t{in_order_index};
            nodes_[location.node_index].keys[location.key_offset] = sorted_keys[in_order_index - 1];
        }
    }

    auto locate(x_t x) const noexcept -> result_t
    {
        auto index = int_t{0};
        auto origin = x_t{0};

        for (auto depth = int_t{0}; depth < depth_max; ++depth)
        {
            auto const& keys = nodes_[index].keys;
            origin = (x >= keys[0]) ? keys[0] : origin;
            origin = (x >= keys[1]) ? keys[1] : origin;
            origin = (x >= keys[2]) ? keys[2] : origin;

            auto const child_offset = int_t{(x >= keys[0]) + (x >= keys[1]) + (x >= keys[2])};
            index = branching_factor * index + 1 + child_offset;
        }

        return {.index = index - node_count, .origin = origin};
    }

    auto locate_with_segment_range(x_t x) const noexcept -> extended_result_t
    {
        auto index = int_t{0};
        auto origin = x_t{0};
        auto end = x_max_;
        auto leaf_index = int_t{0};
        auto leaf_origin = x_t{0};
        auto leaf_end = x_max_;

        for (auto depth = int_t{0}; depth < depth_max; ++depth)
        {
            if (depth == depth_max - 1)
            {
                leaf_index = index - leaf_base_index;
                leaf_origin = origin;
                leaf_end = end;
            }

            auto const& keys = nodes_[index].keys;
            origin = (x >= keys[0]) ? keys[0] : origin;
            origin = (x >= keys[1]) ? keys[1] : origin;
            origin = (x >= keys[2]) ? keys[2] : origin;
            end = (x < keys[2]) ? keys[2] : end;
            end = (x < keys[1]) ? keys[1] : end;
            end = (x < keys[0]) ? keys[0] : end;

            auto const child_offset = int_t{(x >= keys[0]) + (x >= keys[1]) + (x >= keys[2])};
            index = branching_factor * index + 1 + child_offset;
        }

        return {
            .index = index - node_count,
            .origin = origin,
            .end = end,
            .leaf_index = leaf_index,
            .leaf_origin = leaf_origin,
            .leaf_end = leaf_end,
        };
    }

    auto locate_with_leaf_range(x_t x) const noexcept -> extended_result_t
    {
        auto index = int_t{0};
        auto origin = x_t{0};
        auto end = x_max_;

        for (auto depth = int_t{0}; depth < depth_max - 1; ++depth)
        {
            auto const& keys = nodes_[index].keys;
            origin = (x >= keys[0]) ? keys[0] : origin;
            origin = (x >= keys[1]) ? keys[1] : origin;
            origin = (x >= keys[2]) ? keys[2] : origin;
            end = (x < keys[2]) ? keys[2] : end;
            end = (x < keys[1]) ? keys[1] : end;
            end = (x < keys[0]) ? keys[0] : end;

            auto const child_offset = int_t{(x >= keys[0]) + (x >= keys[1]) + (x >= keys[2])};
            index = branching_factor * index + 1 + child_offset;
        }

        auto const leaf_index = index - leaf_base_index;
        auto const leaf_origin = origin;
        auto const leaf_end = end;
        auto const& keys = nodes_[index].keys;
        origin = (x >= keys[0]) ? keys[0] : origin;
        origin = (x >= keys[1]) ? keys[1] : origin;
        origin = (x >= keys[2]) ? keys[2] : origin;
        auto const child_offset = int_t{(x >= keys[0]) + (x >= keys[1]) + (x >= keys[2])};
        index = branching_factor * index + 1 + child_offset;

        return {
            .index = index - node_count,
            .origin = origin,
            .end = {},
            .leaf_index = leaf_index,
            .leaf_origin = leaf_origin,
            .leaf_end = leaf_end,
        };
    }

    auto leaf(int_t leaf_index) const noexcept -> node_t const& { return nodes_[leaf_base_index + leaf_index]; }

    auto x_max() const noexcept -> x_t { return x_max_; }

    auto prefetch_root(static_prefetcher_t const& prefetcher) const noexcept -> void
    {
        prefetcher.prefetch(&nodes_[0]);
    }

    auto prefetch_top(static_prefetcher_t const& prefetcher) const noexcept -> void
    {
        prefetcher.template prefetch<2>(&nodes_[0]);
    }

    auto prefetch_leaf(static_prefetcher_t const& prefetcher, int_t leaf_index) const noexcept -> void
    {
        prefetcher.prefetch(&nodes_[leaf_base_index + leaf_index]);
    }

private:
    struct node_location_t
    {
        int_t node_index;
        int_t key_offset;

        explicit node_location_t(int_t in_order_index) noexcept
        {
            static constexpr auto branching_mask = branching_factor - 1;
            auto const height_above_floor = std::countr_zero(int_cast<uint_t>(in_order_index)) >> 1;
            auto const key_offset_in_row = in_order_index >> (2 * height_above_floor);
            auto const node_offset_in_row = key_offset_in_row >> 2;
            auto const depth_below_root = depth_max - 1 - height_above_floor;
            auto const row_base_index = ((1 << (2 * depth_below_root)) - 1) / node_key_count;
            node_index = row_base_index + node_offset_in_row;
            key_offset = (key_offset_in_row & branching_mask) - 1;
        }
    };

    x_t x_max_{};
    [[maybe_unused]] int_t segment_count_ = max_segment_count;
    alignas(64) nodes_t nodes_{};
};

static_assert(sizeof(benchmark_locator_t) == sizeof(production_locator_t));
static_assert(alignof(benchmark_locator_t) == alignof(production_locator_t));

struct alignas(32) segment_t
{
    std::array<uint64_t, 4> words{};
};
static_assert(sizeof(segment_t) == 32);

using segments_t = std::array<segment_t, benchmark_locator_t::max_segment_count>;

struct spline_data_t
{
    benchmark_locator_t locator{};
    alignas(64) segments_t segments{};
    std::array<uint64_t, 4> tangent{};
};

static_assert(sizeof(spline_data_t) == 11'072);
static_assert(alignof(spline_data_t) == 64);

struct index_hint_t
{
    int_t segment_index{};
};

struct segment_hint_t
{
    int_t segment_index{};
    x_t origin{};
    x_t end{};
};

struct leaf_hint_t
{
    int_t segment_index{};
    x_t origin{};
    x_t end{};
};

struct adaptive_leaf_hint_t
{
    x_t origin{};
    x_t end{};
    uint8_t segment_index{};
    uint8_t confidence{};
};

static_assert(sizeof(index_hint_t) == 8);
static_assert(sizeof(segment_hint_t) == 24);
static_assert(sizeof(leaf_hint_t) == 24);
static_assert(sizeof(adaptive_leaf_hint_t) == 24);

struct timer_state_t
{
    uint64_t previous_timestamp{};
    bool initialized = false;
};

struct filter_state_t
{
    uint64_t output{};
};

struct accumulator_state_t
{
    uint64_t x{};
    uint64_t y{};
};

struct config_t
{
    std::array<uint64_t, 6> words{};
};

struct framing_state_t
{
    bool synchronized = true;
};

struct alignas(64) control_t
{
    config_t config{};
    framing_state_t framing{};
};

struct alignas(64) runtime_state_t
{
    timer_state_t timer{};
    filter_state_t filter{};
    accumulator_state_t accumulator{};
    leaf_hint_t gain_hint{};
};

static_assert(sizeof(timer_state_t) == 16);
static_assert(sizeof(filter_state_t) == 8);
static_assert(sizeof(accumulator_state_t) == 16);
static_assert(sizeof(config_t) == 48);
static_assert(sizeof(control_t) == 64);
static_assert(offsetof(control_t, framing) == 48);
static_assert(sizeof(runtime_state_t) == 64);
static_assert(offsetof(runtime_state_t, gain_hint) == 40);

struct alignas(64) pipeline_data_t
{
    control_t control{};
    runtime_state_t state{};
    spline_data_t spline{};
};

static_assert(offsetof(pipeline_data_t, control) == 0);
static_assert(offsetof(pipeline_data_t, state) == 64);
static_assert(offsetof(pipeline_data_t, spline) == 128);
static_assert(sizeof(pipeline_data_t) == 11'200);

constexpr auto segment_width = int_t{2};
constexpr auto x_max = x_t{benchmark_locator_t::max_segment_count * segment_width};

auto prefetch_segment_neighbors(
    spline_data_t const& data, int_t segment_index, static_prefetcher_t const& prefetcher) noexcept -> void;
auto prefetch_leaf_segment_lines(
    spline_data_t const& data, int_t segment_index, static_prefetcher_t const& prefetcher) noexcept -> void;
auto evaluate_segment(segment_t const& segment, x_t x, x_t origin) noexcept -> uint64_t;

struct baseline_policy_t
{
    using hint_t = index_hint_t;
    static constexpr auto name = std::string_view{"index-prefetch"};

    static auto prefetch(spline_data_t const& data, hint_t const& hint, static_prefetcher_t const& prefetcher) noexcept
        -> void
    {
        data.locator.prefetch_top(prefetcher);
        prefetch_segment_neighbors(data, hint.segment_index, prefetcher);
    }

    static auto evaluate(spline_data_t const& data, hint_t& hint, x_t x) noexcept -> uint64_t
    {
        auto const location = data.locator.locate(x);
        hint.segment_index = location.index;
        return evaluate_segment(data.segments[location.index], x, location.origin);
    }
};

struct segment_hint_policy_t
{
    using hint_t = segment_hint_t;
    static constexpr auto name = std::string_view{"segment-range"};

    static auto prefetch(spline_data_t const& data, hint_t const& hint, static_prefetcher_t const& prefetcher) noexcept
        -> void
    {
        data.locator.prefetch_top(prefetcher);
        prefetch_segment_neighbors(data, hint.segment_index, prefetcher);
    }

    static auto evaluate(spline_data_t const& data, hint_t& hint, x_t x) noexcept -> uint64_t
    {
        if (hint.origin <= x && x < hint.end)
            return evaluate_segment(data.segments[hint.segment_index], x, hint.origin);

        auto const location = data.locator.locate_with_segment_range(x);
        hint = {.segment_index = location.index, .origin = location.origin, .end = location.end};
        return evaluate_segment(data.segments[location.index], x, location.origin);
    }
};

struct full_hint_policy_t
{
    using hint_t = leaf_hint_t;
    static constexpr auto name = std::string_view{"production-full"};

    static auto prefetch(spline_data_t const& data, hint_t const& hint, static_prefetcher_t const& prefetcher) noexcept
        -> void
    {
        data.locator.prefetch_top(prefetcher);
        prefetch_segment_neighbors(data, hint.segment_index, prefetcher);
    }

    static auto evaluate(spline_data_t const& data, hint_t& hint, x_t x) noexcept -> uint64_t
    {
        auto const location = data.locator.locate_with_leaf_range(x);
        hint = {.segment_index = location.index, .origin = location.leaf_origin, .end = location.leaf_end};
        return evaluate_segment(data.segments[location.index], x, location.origin);
    }
};

template <int_t fallback_cache_lines> struct leaf_hint_policy_t
{
    using hint_t = leaf_hint_t;
    static constexpr auto name = fallback_cache_lines == 0 ? std::string_view{"leaf-range"}
        : fallback_cache_lines == 1                        ? std::string_view{"leaf-range+root"}
                                                           : std::string_view{"leaf-range+top2"};

    static auto prefetch(spline_data_t const& data, hint_t const& hint, static_prefetcher_t const& prefetcher) noexcept
        -> void
    {
        if constexpr (fallback_cache_lines == 1) data.locator.prefetch_root(prefetcher);
        if constexpr (fallback_cache_lines == 2) data.locator.prefetch_top(prefetcher);

        auto const leaf_index = hint.segment_index / benchmark_locator_t::branching_factor;
        data.locator.prefetch_leaf(prefetcher, leaf_index);
        prefetch_segment_neighbors(data, hint.segment_index, prefetcher);
    }

    static auto evaluate(spline_data_t const& data, hint_t& hint, x_t x) noexcept -> uint64_t
    {
        auto const leaf_index = hint.segment_index / benchmark_locator_t::branching_factor;
        if (hint.origin <= x && x < hint.end)
        {
            auto const& keys = data.locator.leaf(leaf_index).keys;
            auto origin = hint.origin;
            origin = (x >= keys[0]) ? keys[0] : origin;
            origin = (x >= keys[1]) ? keys[1] : origin;
            origin = (x >= keys[2]) ? keys[2] : origin;
            auto const child_offset = int_t{(x >= keys[0]) + (x >= keys[1]) + (x >= keys[2])};
            auto const segment_index = leaf_index * benchmark_locator_t::branching_factor + child_offset;
            hint.segment_index = segment_index;
            return evaluate_segment(data.segments[segment_index], x, origin);
        }

        auto const location = data.locator.locate_with_leaf_range(x);
        hint = {.segment_index = location.index, .origin = location.leaf_origin, .end = location.leaf_end};
        return evaluate_segment(data.segments[location.index], x, location.origin);
    }
};

using leaf_hint_policy = leaf_hint_policy_t<0>;
using leaf_hint_root_policy = leaf_hint_policy_t<1>;
using leaf_hint_top2_policy = leaf_hint_policy_t<2>;

struct leaf_lines_policy_t
{
    using hint_t = leaf_hint_t;
    static constexpr auto name = std::string_view{"leaf-lines"};

    static auto prefetch(spline_data_t const& data, hint_t const& hint, static_prefetcher_t const& prefetcher) noexcept
        -> void
    {
        auto const leaf_index = hint.segment_index / benchmark_locator_t::branching_factor;
        data.locator.prefetch_leaf(prefetcher, leaf_index);
        prefetch_leaf_segment_lines(data, hint.segment_index, prefetcher);
    }

    static auto evaluate(spline_data_t const& data, hint_t& hint, x_t x) noexcept -> uint64_t
    {
        return leaf_hint_policy::evaluate(data, hint, x);
    }
};

template <uint8_t max_confidence, uint8_t leaf_threshold> struct adaptive_leaf_policy_t
{
    static_assert(0 < leaf_threshold && leaf_threshold <= max_confidence);

    using hint_t = adaptive_leaf_hint_t;
    static constexpr auto name
        = max_confidence == 1 ? std::string_view{"adaptive-1bit"} : std::string_view{"adaptive-2bit"};

    static auto prefetch(spline_data_t const& data, hint_t const& hint, static_prefetcher_t const& prefetcher) noexcept
        -> void
    {
        auto const segment_index = int_cast<int_t>(hint.segment_index);
        if (hint.confidence >= leaf_threshold)
        {
            auto const leaf_index = segment_index / benchmark_locator_t::branching_factor;
            data.locator.prefetch_leaf(prefetcher, leaf_index);
        }
        else
        {
            data.locator.prefetch_top(prefetcher);
        }
        prefetch_segment_neighbors(data, segment_index, prefetcher);
    }

    static auto evaluate(spline_data_t const& data, hint_t& hint, x_t x) noexcept -> uint64_t
    {
        auto const leaf_hit = hint.origin <= x && x < hint.end;
        auto const use_leaf = hint.confidence >= leaf_threshold;
        auto result = uint64_t{};

        if (use_leaf && leaf_hit)
        {
            auto const leaf_index = int_cast<int_t>(hint.segment_index) / benchmark_locator_t::branching_factor;
            auto const& keys = data.locator.leaf(leaf_index).keys;
            auto origin = hint.origin;
            origin = (x >= keys[0]) ? keys[0] : origin;
            origin = (x >= keys[1]) ? keys[1] : origin;
            origin = (x >= keys[2]) ? keys[2] : origin;
            auto const child_offset = int_t{(x >= keys[0]) + (x >= keys[1]) + (x >= keys[2])};
            auto const segment_index = leaf_index * benchmark_locator_t::branching_factor + child_offset;
            hint.segment_index = int_cast<uint8_t>(segment_index);
            result = evaluate_segment(data.segments[segment_index], x, origin);
        }
        else
        {
            auto const location = data.locator.locate_with_leaf_range(x);
            hint.segment_index = int_cast<uint8_t>(location.index);
            hint.origin = location.leaf_origin;
            hint.end = location.leaf_end;
            result = evaluate_segment(data.segments[location.index], x, location.origin);
        }

        if (leaf_hit)
        {
            if (hint.confidence < max_confidence) ++hint.confidence;
        }
        else if (hint.confidence != 0) { --hint.confidence; }

        return result;
    }
};

using adaptive_leaf_1bit_policy = adaptive_leaf_policy_t<1, 1>;
using adaptive_leaf_2bit_policy = adaptive_leaf_policy_t<3, 2>;

template <bool prefetch_control, bool prefetch_state> struct entry_prefetch_policy_t
{
    static constexpr auto has_entry_prefetch = prefetch_control || prefetch_state;
    static constexpr auto has_staged_prefetch = false;
    static constexpr auto name = !prefetch_control && !prefetch_state ? std::string_view{"none"}
        : prefetch_control && !prefetch_state                         ? std::string_view{"control"}
        : !prefetch_control && prefetch_state                         ? std::string_view{"state"}
                                                                      : std::string_view{"control+state"};

    static auto prefetch(pipeline_data_t const& pipeline, static_prefetcher_t const& prefetcher) noexcept -> void
    {
        if constexpr (prefetch_control) prefetcher.prefetch(&pipeline.control);
        if constexpr (prefetch_state) prefetcher.prefetch(&pipeline.state);
    }

    static auto after_synchronized(pipeline_data_t const&, static_prefetcher_t const&) noexcept -> void {}
};

using no_entry_prefetch_policy = entry_prefetch_policy_t<false, false>;
using control_prefetch_policy = entry_prefetch_policy_t<true, false>;
using state_prefetch_policy = entry_prefetch_policy_t<false, true>;
using control_state_prefetch_policy = entry_prefetch_policy_t<true, true>;

struct state_control_prefetch_policy
{
    [[maybe_unused]] static constexpr auto has_entry_prefetch = true;
    [[maybe_unused]] static constexpr auto has_staged_prefetch = false;
    static constexpr auto name = std::string_view{"state+control"};

    static auto prefetch(pipeline_data_t const& pipeline, static_prefetcher_t const& prefetcher) noexcept -> void
    {
        prefetcher.prefetch(&pipeline.state);
        prefetcher.prefetch(&pipeline.control);
    }

    static auto after_synchronized(pipeline_data_t const&, static_prefetcher_t const&) noexcept -> void {}
};

struct staged_prefetch_policy
{
    static constexpr auto has_entry_prefetch = true;
    static constexpr auto has_staged_prefetch = true;
    static constexpr auto name = std::string_view{"staged"};

    static auto prefetch(pipeline_data_t const& pipeline, static_prefetcher_t const& prefetcher) noexcept -> void
    {
        prefetcher.prefetch(&pipeline.control);
    }

    static auto after_synchronized(pipeline_data_t const& pipeline, static_prefetcher_t const& prefetcher) noexcept
        -> void
    {
        prefetcher.prefetch(&pipeline.state);
    }
};

struct no_spline_extra_prefetch_policy
{
    static constexpr auto name = std::string_view{"none"};

    static auto prefetch(pipeline_data_t const&, static_prefetcher_t const&) noexcept -> void {}
};

struct tangent_prefetch_policy
{
    static constexpr auto name = std::string_view{"tangent"};

    static auto prefetch(pipeline_data_t const& pipeline, static_prefetcher_t const& prefetcher) noexcept -> void
    {
        prefetcher.prefetch(&pipeline.spline.tangent);
    }
};

struct header_prefetch_policy
{
    static constexpr auto name = std::string_view{"header"};

    static auto prefetch(pipeline_data_t const& pipeline, static_prefetcher_t const& prefetcher) noexcept -> void
    {
        prefetcher.prefetch(&pipeline.spline.locator);
    }
};

enum class motion_t
{
    stationary,
    segment_walk,
    leaf_walk,
    random,
};

constexpr auto motion_name(motion_t motion) noexcept -> std::string_view
{
    switch (motion)
    {
        case motion_t::stationary: return "stationary";
        case motion_t::segment_walk: return "segment-walk";
        case motion_t::leaf_walk: return "leaf-walk";
        case motion_t::random: return "random";
    }
    __builtin_unreachable();
}

constexpr auto mix(uint64_t value) noexcept -> uint64_t
{
    value ^= value >> 30;
    value *= uint64_t{0xbf58476d1ce4e5b9};
    value ^= value >> 27;
    value *= uint64_t{0x94d049bb133111eb};
    return value ^ (value >> 31);
}

constexpr auto segment_for(motion_t motion, std::size_t instance_index, std::size_t rotation) noexcept -> int_t
{
    auto const base = int_cast<int_t>(mix(instance_index) & 0xffu);
    switch (motion)
    {
        case motion_t::stationary: return base;
        case motion_t::segment_walk: return (base + int_cast<int_t>(rotation)) & 0xff;
        case motion_t::leaf_walk: return (base + 4 * int_cast<int_t>(rotation)) & 0xff;
        case motion_t::random: return int_cast<int_t>(mix(instance_index ^ (rotation * 0x9e3779b97f4a7c15ull)) & 0xffu);
    }
    __builtin_unreachable();
}

constexpr auto sample_x(motion_t motion, std::size_t instance_index, std::size_t rotation) noexcept -> x_t
{
    return x_t{segment_for(motion, instance_index, rotation) * segment_width + 1};
}

struct framing_inputs_t
{
    std::size_t count;
    std::size_t max_vals;
    std::size_t num_vals;
    uint64_t timestamp;
};

constexpr auto make_framing_inputs(std::size_t instance_index, std::size_t rotation) noexcept -> framing_inputs_t
{
    auto const variability = instance_index + rotation;
    return {
        .count = std::size_t{3} + (variability & 1u),
        .max_vals = std::size_t{8} + ((variability >> 1) & 31u),
        .num_vals = (variability >> 3) & 31u,
        .timestamp = (uint64_t{rotation} << 20) + (uint64_t{instance_index} & 0x3ffu),
    };
}

constexpr auto make_normal_framing_inputs(std::size_t instance_index, std::size_t rotation) noexcept -> framing_inputs_t
{
    return {
        .count = 4,
        .max_vals = std::size_t{16} + ((instance_index >> 1) & 15u),
        .num_vals = std::size_t{4} + ((instance_index + rotation) & 3u),
        .timestamp = (uint64_t{rotation} << 20) + (uint64_t{instance_index} & 0x3ffu),
    };
}

auto make_report_values(std::size_t instance_index, std::size_t rotation) noexcept -> std::array<crv_input_value_t, 4>
{
    auto const mixed = mix(instance_index ^ (rotation * 0x9e3779b97f4a7c15ull));
    auto const x = static_cast<int32_t>((mixed & 0x3ffu) - 0x200u);
    auto const y = static_cast<int32_t>(((mixed >> 10) & 0x3ffu) - 0x200u);

    return {{
        {.type = CRV_EV_REL, .code = 8, .value = 1},
        {.type = CRV_EV_REL, .code = CRV_REL_X, .value = x},
        {.type = CRV_EV_REL, .code = CRV_REL_Y, .value = y},
        {.type = CRV_EV_SYN, .code = CRV_SYN_REPORT, .value = 0},
    }};
}

auto prefetch_segment_neighbors(
    spline_data_t const& data, int_t segment_index, static_prefetcher_t const& prefetcher) noexcept -> void
{
    auto const base_address = reinterpret_cast<std::uintptr_t>(data.segments.data());
    auto const offset = sizeof(segment_t);
    prefetcher.prefetch(reinterpret_cast<void const*>(base_address + (segment_index - 1) * offset));
    prefetcher.prefetch(reinterpret_cast<void const*>(base_address + (segment_index + 1) * offset));
}

auto prefetch_leaf_segment_lines(
    spline_data_t const& data, int_t segment_index, static_prefetcher_t const& prefetcher) noexcept -> void
{
    auto const leaf_base = segment_index - segment_index % benchmark_locator_t::branching_factor;
    prefetcher.prefetch(&data.segments[int_cast<std::size_t>(leaf_base)]);
    prefetcher.prefetch(&data.segments[int_cast<std::size_t>(leaf_base + 2)]);
}

auto evaluate_segment(segment_t const& segment, x_t x, x_t origin) noexcept -> uint64_t
{
    auto value = segment.words[0] ^ segment.words[1];
    value += segment.words[2] ^ segment.words[3];
    value ^= int_cast<uint64_t>(x.value);
    value += int_cast<uint64_t>(origin.value);
    return value;
}

auto make_sorted_keys() -> std::array<x_t, benchmark_locator_t::total_key_count>
{
    auto keys = std::array<x_t, benchmark_locator_t::total_key_count>{};
    for (auto index = std::size_t{0}; index < keys.size(); ++index)
        keys[index] = x_t{int_cast<int_t>(index + 1) * segment_width};
    return keys;
}

auto recover_sorted_keys(production_locator_t const& locator)
    -> std::optional<std::array<x_t, benchmark_locator_t::total_key_count>>
{
    if (!locator.is_valid()) return std::nullopt;

    auto keys = std::array<x_t, benchmark_locator_t::total_key_count>{};
    auto const segment_count = locator.segment_count();
    auto const domain_end = locator.x_max();
    auto previous = typename x_t::value_t{};

    for (auto segment_index = int_t{1}; segment_index < segment_count; ++segment_index)
    {
        auto lower = previous + 1;
        auto upper = domain_end.value;

        while (lower < upper)
        {
            auto const midpoint = lower + (upper - lower) / 2;
            auto const location = locator.locate(x_t::literal(midpoint));
            if (location.index < segment_index) lower = midpoint + 1;
            else upper = midpoint;
        }

        auto const breakpoint = x_t::literal(lower);
        auto const location = locator.locate(breakpoint);
        if (location.index != segment_index || location.origin != breakpoint) return std::nullopt;

        keys[int_cast<std::size_t>(segment_index - 1)] = breakpoint;
        previous = lower;
    }

    auto const padding_begin = int_cast<std::size_t>(max(int_t{0}, segment_count - 1));
    for (auto index = padding_begin; index < keys.size(); ++index) keys[index] = domain_end;

    return keys;
}

auto make_prototype() -> spline_data_t
{
    auto result = spline_data_t{};
    auto const keys = make_sorted_keys();
    result.locator = benchmark_locator_t{keys, x_max};

    for (auto index = std::size_t{0}; index < result.segments.size(); ++index)
    {
        auto const seed = mix(index + 1);
        result.segments[index].words = {seed, mix(seed), mix(seed + 1), mix(seed + 2)};
    }

    result.tangent = {1, 2, 3, 4};
    return result;
}

auto make_prototype(production_locator_t const& locator) -> std::optional<spline_data_t>
{
    auto const recovered_keys = recover_sorted_keys(locator);
    if (!recovered_keys) return std::nullopt;

    auto result = spline_data_t{};
    result.locator = benchmark_locator_t{*recovered_keys, locator.x_max()};

    for (auto index = std::size_t{0}; index < result.segments.size(); ++index)
    {
        auto const seed = mix(index + 1);
        result.segments[index].words = {seed, mix(seed), mix(seed + 1), mix(seed + 2)};
    }

    result.tangent = {1, 2, 3, 4};
    return result;
}

auto make_pipeline_prototype() -> pipeline_data_t
{
    auto result = pipeline_data_t{};
    result.control.config.words = {
        0x243f6a8885a308d3ull,
        0x13198a2e03707344ull,
        0xa4093822299f31d0ull,
        0x082efa98ec4e6c89ull,
        0x452821e638d01377ull,
        0xbe5466cf34e90c6cull,
    };
    result.spline = make_prototype();
    return result;
}

auto make_pipeline_prototype(production_locator_t const& locator) -> std::optional<pipeline_data_t>
{
    auto const spline = make_prototype(locator);
    if (!spline) return std::nullopt;

    auto result = pipeline_data_t{};
    result.control.config.words = {
        0x243f6a8885a308d3ull,
        0x13198a2e03707344ull,
        0xa4093822299f31d0ull,
        0x082efa98ec4e6c89ull,
        0x452821e638d01377ull,
        0xbe5466cf34e90c6cull,
    };
    result.spline = *spline;
    return result;
}

auto verify_locator() -> bool
{
    auto const keys = make_sorted_keys();
    auto const production = production_locator_t{keys, x_max, production_locator_t::max_segment_count};
    auto const benchmark = benchmark_locator_t{keys, x_max};

    for (auto raw = int_t{0}; raw < benchmark_locator_t::max_segment_count * segment_width; ++raw)
    {
        auto const x = x_t{raw};
        auto const expected = production.locate(x);
        auto production_hint = production_locator_t::hint_t{};
        auto const hinted_expected = production.locate(x, production_hint, false);
        auto const actual = benchmark.locate(x);
        auto const extended = benchmark.locate_with_segment_range(x);
        auto const leaf_extended = benchmark.locate_with_leaf_range(x);
        if (actual.index != expected.index || actual.origin != expected.origin) return false;
        if (hinted_expected != expected) return false;
        if (extended.index != expected.index || extended.origin != expected.origin) return false;
        if (!(extended.origin <= x && x < extended.end)) return false;
        if (!(extended.leaf_origin <= x && x < extended.leaf_end)) return false;
        if (extended.index / benchmark_locator_t::branching_factor != extended.leaf_index) return false;
        if (leaf_extended.index != expected.index || leaf_extended.origin != expected.origin) return false;
        if (!(leaf_extended.leaf_origin <= x && x < leaf_extended.leaf_end)) return false;
        if (leaf_extended.index / benchmark_locator_t::branching_factor != leaf_extended.leaf_index) return false;
        if (production_hint.segment_index != leaf_extended.index
            || production_hint.leaf_origin != leaf_extended.leaf_origin
            || production_hint.leaf_end != leaf_extended.leaf_end)
        {
            return false;
        }
    }

    return true;
}

auto verify_locator(production_locator_t const& production, benchmark_locator_t const& benchmark) -> bool
{
    auto const segment_count = production.segment_count();
    if (segment_count <= 0) return false;

    auto const recovered_keys = recover_sorted_keys(production);
    if (!recovered_keys) return false;

    auto const matches = [&](x_t x) {
        auto hint = production_locator_t::hint_t{};
        auto const expected = production.locate(x, hint, false);
        auto const actual = benchmark.locate_with_leaf_range(x);
        return actual.index == expected.index && actual.origin == expected.origin && hint.segment_index == actual.index
            && hint.leaf_origin == actual.leaf_origin && hint.leaf_end == actual.leaf_end;
    };

    auto origin = x_t{};
    for (auto segment_index = int_t{0}; segment_index < segment_count; ++segment_index)
    {
        auto const at_origin = production.locate(origin);
        auto const benchmark_at_origin = benchmark.locate(origin);
        if (at_origin.index != segment_index || benchmark_at_origin.index != at_origin.index
            || benchmark_at_origin.origin != at_origin.origin)
        {
            return false;
        }
        if (!matches(origin)) return false;

        if (segment_index + 1 < segment_count)
        {
            auto const next_origin = (*recovered_keys)[int_cast<std::size_t>(segment_index)];
            if (next_origin.value <= origin.value) return false;

            auto const before_next = x_t::literal(next_origin.value - 1);
            auto const expected_before_next = production.locate(before_next);
            auto const actual_before_next = benchmark.locate(before_next);
            if (actual_before_next.index != expected_before_next.index
                || actual_before_next.origin != expected_before_next.origin)
            {
                return false;
            }
            if (!matches(before_next)) return false;

            origin = next_origin;
        }
    }

    auto const last = x_t::literal(production.x_max().value - 1);
    auto const expected_last = production.locate(last);
    auto const actual_last = benchmark.locate(last);
    return actual_last.index == expected_last.index && actual_last.origin == expected_last.origin && matches(last);
}

constexpr auto capture_stream_error_name(capture_stream_error_code_t code) noexcept -> std::string_view
{
    switch (code)
    {
        case capture_stream_error_code_t::interrupted: return "interrupted";
        case capture_stream_error_code_t::source_disconnected: return "source disconnected";
        case capture_stream_error_code_t::source_read_failed: return "source read failed";
        case capture_stream_error_code_t::truncated_stream_header: return "truncated stream header";
        case capture_stream_error_code_t::invalid_magic: return "invalid magic";
        case capture_stream_error_code_t::unsupported_format_version: return "unsupported format version";
        case capture_stream_error_code_t::invalid_stream_header_size: return "invalid stream header size";
        case capture_stream_error_code_t::stream_header_too_large: return "stream header too large";
        case capture_stream_error_code_t::unsupported_input_value_size: return "unsupported input value size";
        case capture_stream_error_code_t::unsupported_clock: return "unsupported capture clock";
        case capture_stream_error_code_t::unsupported_byte_order: return "unsupported byte order";
        case capture_stream_error_code_t::unsupported_stream_flags: return "unsupported stream flags";
        case capture_stream_error_code_t::truncated_frame: return "truncated frame";
        case capture_stream_error_code_t::invalid_frame_size: return "invalid frame size";
        case capture_stream_error_code_t::invalid_frame_header_size: return "invalid frame header size";
        case capture_stream_error_code_t::frame_too_large: return "frame too large";
        case capture_stream_error_code_t::invalid_input_values_header_size: return "invalid input values header size";
        case capture_stream_error_code_t::invalid_input_value_count: return "invalid input value count";
        case capture_stream_error_code_t::invalid_input_value_capacity: return "invalid input value capacity";
        case capture_stream_error_code_t::input_value_capacity_too_large: return "input value capacity too large";
        case capture_stream_error_code_t::inconsistent_input_values_frame_size:
            return "inconsistent input values frame size";
    }
    return "unknown capture stream error";
}

struct replay_report_t
{
    int32_t x{};
    int32_t y{};
};

auto decode_report(capture_input_values_view_t const& frame) noexcept -> std::optional<replay_report_t>
{
    if (frame.values.empty()) return std::nullopt;

    auto const& terminator = frame.values.back();
    if (terminator.type != CRV_EV_SYN || terminator.code != CRV_SYN_REPORT) return std::nullopt;

    auto result = replay_report_t{};
    auto has_x = false;
    auto has_y = false;

    for (auto const& value : frame.values.first(frame.values.size() - 1))
    {
        if (value.type != CRV_EV_REL) continue;

        if (value.code == CRV_REL_X)
        {
            if (has_x) return std::nullopt;
            result.x = value.value;
            has_x = true;
        }
        else if (value.code == CRV_REL_Y)
        {
            if (has_y) return std::nullopt;
            result.y = value.value;
            has_y = true;
        }
    }

    return result;
}

auto is_capture_split(capture_input_values_view_t const& frame) noexcept -> bool
{
    if (frame.values.empty()) return false;
    auto const& terminator = frame.values.back();
    return terminator.type == CRV_EV_SYN && terminator.code == CRV_SYN_REPORT && terminator.value != 0;
}

struct replay_configuration_t
{
    production_spline_t spline{};
    replay_velocity_t::scale_t velocity_scale{};
    replay_duration_t half_life{};
    int_t dpi{};
    float_t half_life_ms{};
};

auto load_replay_configuration(char const* path, replay_configuration_t& result) -> bool
{
    auto root = model::root_t{};

    try
    {
        serialization::tomlpp::deserializer_t{}(path, root);
    }
    catch (std::exception const& exception)
    {
        std::cerr << path << ": config read failed: " << exception.what() << '\n';
        return false;
    }

    result.dpi = root.device.dpi.value();
    result.half_life_ms = root.profile.filter_halflife.value();

    if (result.dpi <= 0)
    {
        std::cerr << path << ": dpi must be positive for capture replay\n";
        return false;
    }
    if (!std::isfinite(result.half_life_ms) || result.half_life_ms < 0)
    {
        std::cerr << path << ": filter half-life must be finite and nonnegative\n";
        return false;
    }

    result.velocity_scale
        = to_fixed<replay_velocity_t::scale_t>(float_t{1'000'000'000} / static_cast<float_t>(result.dpi));
    result.half_life = to_fixed<replay_duration_t>(result.half_life_ms * float_t{1'000'000});

    using curve_t = decltype(model::curves::create_composed_curve<float_t>(model::curves::synchronous_t::config_t{}));
    auto curve = curve_t{model::curves::create_composed_curve<float_t>(model::curves::synchronous_t::config_t{})};

    auto const active_curve = static_cast<std::size_t>(root.profile.curves.active.value());
    if (active_curve >= model::curves::curves_count)
    {
        std::cerr << path << ": active curve id is out of range\n";
        return false;
    }

    tuple::visit_at(root.profile.curves.configs, active_curve, [&](auto const& curve_config) {
        curve = model::curves::create_composed_curve<float_t>(curve_config.specific);
    });

    auto const generation
        = spline_factory_t{}(result.spline, spline::gain_curve_target_t{curve}, float_t{2e-6}, std::vector<x_t>{});
    if (!generation)
    {
        auto const& error = *generation.error;
        std::cerr << path << ": spline generation failed over [" << from_fixed<float_t>(error.left) << ", "
                  << from_fixed<float_t>(error.right) << ")\n";
        return false;
    }

    return true;
}

struct replay_trace_t
{
    std::vector<x_t> speed;
    std::vector<uint64_t> epoch;
    std::size_t frame_count{};
    std::size_t invalid_report_count{};
    std::size_t split_frame_count{};
    std::size_t resynchronization_count{};
    std::size_t invalid_timestamp_count{};
    std::size_t velocity_out_of_range_count{};
};

auto load_replay_trace(char const* path, replay_configuration_t const& config, replay_trace_t& result) -> bool
{
    auto opened = open_capture_file(path);
    if (!opened)
    {
        std::cerr << path << ": capture open failed";
        if (opened.error().system_error != 0) std::cerr << ": " << std::strerror(opened.error().system_error);
        std::cerr << '\n';
        return false;
    }

    auto stream = std::move(*opened);
    auto timer = replay_timer_t{};
    auto filter = replay_filter_t{};
    auto velocity = replay_velocity_t{};
    auto synchronized = true;
    auto previous_sequence = std::optional<uint64_t>{};
    auto epoch = uint64_t{};

    for (;;)
    {
        auto frame_result = stream.read_input_values();
        if (!frame_result)
        {
            auto const& error = frame_result.error();
            std::cerr << path << ": capture decode failed at byte " << error.stream_offset << ": "
                      << capture_stream_error_name(error.code);
            if (error.system_error != 0) std::cerr << ": " << std::strerror(error.system_error);
            std::cerr << '\n';
            return false;
        }
        if (!frame_result->has_value()) break;

        auto const& frame = frame_result->value();
        ++result.frame_count;

        if (previous_sequence && frame.sequence != *previous_sequence + 1)
        {
            std::cerr << path << ": capture sequence discontinuity: expected " << (*previous_sequence + 1)
                      << ", received " << frame.sequence << '\n';
            return false;
        }
        previous_sequence = frame.sequence;

        if (is_capture_split(frame))
        {
            ++result.split_frame_count;
            synchronized = false;
            continue;
        }

        if (!synchronized)
        {
            timer = {};
            filter = {};
            (void)timer(frame.timestamp_ns);
            synchronized = true;
            ++epoch;
            ++result.resynchronization_count;
            continue;
        }

        auto const report = decode_report(frame);
        if (!report)
        {
            ++result.invalid_report_count;
            continue;
        }

        auto const timing = timer(frame.timestamp_ns);
        if (timing.status == replay_timer_t::status_t::initial) continue;
        if (timing.status != replay_timer_t::status_t::ready)
        {
            ++result.invalid_timestamp_count;
            continue;
        }

        auto const speed = velocity(report->x, report->y, timing.duration, config.velocity_scale);
        if (!speed.valid)
        {
            ++result.velocity_out_of_range_count;
            continue;
        }

        auto const filtered = config.half_life == replay_duration_t{}
            ? speed.value
            : filter(speed.value, config.half_life, timing.duration);
        result.speed.push_back(filtered);
        result.epoch.push_back(epoch);
    }

    if (result.speed.size() < 2)
    {
        std::cerr << path << ": capture produced fewer than two spline-input samples\n";
        return false;
    }

    return true;
}

template <typename policy_t>
auto evaluate_entry(pipeline_data_t& pipeline, std::size_t instance_index, std::size_t rotation, motion_t motion,
    static_prefetcher_t const& prefetcher) noexcept -> uint64_t
{
    policy_t::prefetch(pipeline, prefetcher);

    auto const framing = make_framing_inputs(instance_index, rotation);
    auto const forced_split = framing.max_vals > 1 && framing.num_vals >= framing.max_vals - 1;
    auto const invalid_count = framing.count > framing.max_vals;
    auto const framing_bits = uint64_t{forced_split} | (uint64_t{invalid_count} << 1);

    if (!pipeline.control.framing.synchronized) [[unlikely]]
        return framing_bits;

    leaf_hint_root_policy::prefetch(pipeline.spline, pipeline.state.gain_hint, prefetcher);

    auto& state = pipeline.state;
    auto const duration = state.timer.initialized ? framing.timestamp - state.timer.previous_timestamp : uint64_t{};
    state.timer.previous_timestamp = framing.timestamp;
    state.timer.initialized = true;

    auto const x = sample_x(motion, instance_index, rotation);
    auto const config_index = (framing.count ^ framing.num_vals) & std::size_t{3};
    auto runway = mix(duration ^ pipeline.control.config.words[config_index] ^ int_cast<uint64_t>(x.value));
    runway ^= mix(state.filter.output + pipeline.control.config.words[4]);
    state.filter.output = runway;

    auto const value = leaf_hint_root_policy::evaluate(pipeline.spline, state.gain_hint, x);
    state.accumulator.x += value ^ runway;
    state.accumulator.y ^= value + pipeline.control.config.words[5];

    return value ^ runway ^ state.accumulator.x ^ state.accumulator.y ^ framing_bits;
}

template <typename entry_policy_t, typename spline_policy_t,
    typename spline_extra_prefetch_policy_t = tangent_prefetch_policy>
auto evaluate_scheduled_entry_with_x(pipeline_data_t& pipeline, std::size_t instance_index, std::size_t rotation, x_t x,
    static_prefetcher_t const& prefetcher) noexcept -> uint64_t
{
    auto values = make_report_values(instance_index, rotation);
    do_not_optimize(values);

    entry_policy_t::prefetch(pipeline, prefetcher);
    if constexpr (entry_policy_t::has_entry_prefetch) clobber_memory();

    auto const framing = make_normal_framing_inputs(instance_index, rotation);
    auto const forced_split = framing.max_vals > 1 && framing.num_vals >= framing.max_vals - 1;
    auto const invalid_count = framing.count > framing.max_vals;
    auto const framing_bits = uint64_t{forced_split} | (uint64_t{invalid_count} << 1);

    if (!pipeline.control.framing.synchronized) [[unlikely]]
        return framing_bits;
    if (forced_split) [[unlikely]]
        return framing_bits;

    entry_policy_t::after_synchronized(pipeline, prefetcher);
    if constexpr (entry_policy_t::has_staged_prefetch) clobber_memory();

    auto adapter = input_value_array_adapter_t{values.data(), values.size()};
    auto frame = pipeline::input_frame_t{adapter, framing.count};
    auto report = pipeline::relative_report_t{frame};
    if (!report.valid()) return framing_bits ^ uint64_t{4};

    spline_extra_prefetch_policy_t::prefetch(pipeline, prefetcher);
    spline_policy_t::prefetch(pipeline.spline, pipeline.state.gain_hint, prefetcher);

    auto& state = pipeline.state;
    auto const duration = state.timer.initialized ? framing.timestamp - state.timer.previous_timestamp : uint64_t{};
    state.timer.previous_timestamp = framing.timestamp;
    state.timer.initialized = true;

    auto const config_index = (framing.count ^ framing.num_vals) & std::size_t{3};
    auto runway = mix(duration ^ pipeline.control.config.words[config_index] ^ int_cast<uint64_t>(x.value));
    runway ^= mix(int_cast<uint64_t>(report.x()) ^ (int_cast<uint64_t>(report.y()) << 1));
    runway ^= mix(state.filter.output + pipeline.control.config.words[4]);
    state.filter.output = runway;

    auto const value = x >= pipeline.spline.locator.x_max()
        ? pipeline.spline.tangent[0] ^ pipeline.spline.tangent[1] ^ pipeline.spline.tangent[2]
            ^ pipeline.spline.tangent[3] ^ int_cast<uint64_t>(x.value)
        : spline_policy_t::evaluate(pipeline.spline, state.gain_hint, x);
    state.accumulator.x += value ^ runway;
    state.accumulator.y ^= value + pipeline.control.config.words[5];

    return value ^ runway ^ state.accumulator.x ^ state.accumulator.y ^ framing_bits;
}

template <typename entry_policy_t, typename spline_policy_t>
auto evaluate_scheduled_entry(pipeline_data_t& pipeline, std::size_t instance_index, std::size_t rotation,
    motion_t motion, static_prefetcher_t const& prefetcher) noexcept -> uint64_t
{
    return evaluate_scheduled_entry_with_x<entry_policy_t, spline_policy_t>(
        pipeline, instance_index, rotation, sample_x(motion, instance_index, rotation), prefetcher);
}

template <typename policy_t>
auto warmup_entry(std::vector<pipeline_data_t>& data, std::vector<std::size_t> const& order, motion_t motion) noexcept
    -> uint64_t
{
    auto result = uint64_t{};
    auto const prefetcher = static_prefetcher_t{};
    for (auto const index : order) result ^= evaluate_entry<policy_t>(data[index], index, 0, motion, prefetcher);
    return result;
}

template <typename policy_t>
auto benchmark_entry(std::vector<pipeline_data_t>& data, std::vector<std::size_t> const& order, motion_t motion,
    std::size_t rotations) -> float_t
{
    for (auto& pipeline : data) pipeline.state = {};
    do_not_optimize(warmup_entry<policy_t>(data, order, motion));

    auto const prefetcher = static_prefetcher_t{};
    auto result = uint64_t{};
    auto aux = uint32_t{};

    _mm_lfence();
    auto const start_cycles = __rdtsc();
    _mm_lfence();

    for (auto rotation = std::size_t{1}; rotation <= rotations; ++rotation)
        for (auto const index : order)
            result ^= evaluate_entry<policy_t>(data[index], index, rotation, motion, prefetcher);

    _mm_lfence();
    auto const end_cycles = __rdtscp(&aux);
    _mm_lfence();

    do_not_optimize(result);
    auto const sample_count = rotations * data.size();
    return static_cast<float_t>(end_cycles - start_cycles) / static_cast<float_t>(sample_count);
}

template <typename policy_t>
auto benchmark_entry_median(std::vector<pipeline_data_t>& data, std::vector<std::size_t> const& order, motion_t motion,
    std::size_t rotations, std::size_t trials) -> float_t
{
    auto results = std::vector<float_t>{};
    results.reserve(trials);
    for (auto trial = std::size_t{0}; trial < trials; ++trial)
        results.push_back(benchmark_entry<policy_t>(data, order, motion, rotations));
    std::ranges::sort(results);
    return results[results.size() / 2];
}

template <typename entry_policy_t, typename spline_policy_t>
auto benchmark_scheduled_entry(std::vector<pipeline_data_t>& data, std::vector<std::size_t> const& order,
    motion_t motion, std::size_t rotations) -> float_t
{
    for (auto& pipeline : data) pipeline.state = {};

    auto const prefetcher = static_prefetcher_t{};
    auto result = uint64_t{};
    for (auto const index : order)
        result ^= evaluate_scheduled_entry<entry_policy_t, spline_policy_t>(data[index], index, 0, motion, prefetcher);
    do_not_optimize(result);

    auto aux = uint32_t{};
    _mm_lfence();
    auto const start_cycles = __rdtsc();
    _mm_lfence();

    for (auto rotation = std::size_t{1}; rotation <= rotations; ++rotation)
        for (auto const index : order)
            result ^= evaluate_scheduled_entry<entry_policy_t, spline_policy_t>(
                data[index], index, rotation, motion, prefetcher);

    _mm_lfence();
    auto const end_cycles = __rdtscp(&aux);
    _mm_lfence();

    do_not_optimize(result);
    auto const sample_count = rotations * data.size();
    return static_cast<float_t>(end_cycles - start_cycles) / static_cast<float_t>(sample_count);
}

template <typename entry_policy_t, typename spline_policy_t>
auto benchmark_scheduled_entry_median(std::vector<pipeline_data_t>& data, std::vector<std::size_t> const& order,
    motion_t motion, std::size_t rotations, std::size_t trials) -> float_t
{
    auto results = std::vector<float_t>{};
    results.reserve(trials);
    for (auto trial = std::size_t{0}; trial < trials; ++trial)
        results.push_back(benchmark_scheduled_entry<entry_policy_t, spline_policy_t>(data, order, motion, rotations));
    std::ranges::sort(results);
    return results[results.size() / 2];
}

template <typename policy_t>
auto warmup(std::vector<spline_data_t> const& data, std::vector<typename policy_t::hint_t>& hints,
    std::vector<std::size_t> const& order, motion_t motion) noexcept -> uint64_t
{
    auto result = uint64_t{};
    auto const prefetcher = static_prefetcher_t{};
    for (auto const index : order)
    {
        policy_t::prefetch(data[index], hints[index], prefetcher);
        result ^= policy_t::evaluate(data[index], hints[index], sample_x(motion, index, 0));
    }
    return result;
}

template <typename policy_t>
auto benchmark(std::vector<spline_data_t> const& data, std::vector<std::size_t> const& order, motion_t motion,
    std::size_t rotations) -> float_t
{
    auto hints = std::vector<typename policy_t::hint_t>(data.size());
    do_not_optimize(warmup<policy_t>(data, hints, order, motion));

    auto const prefetcher = static_prefetcher_t{};
    auto result = uint64_t{};
    auto aux = uint32_t{};

    _mm_lfence();
    auto const start_cycles = __rdtsc();
    _mm_lfence();

    for (auto rotation = std::size_t{1}; rotation <= rotations; ++rotation)
    {
        for (auto const index : order)
        {
            policy_t::prefetch(data[index], hints[index], prefetcher);
            auto const x = sample_x(motion, index, rotation);
            result ^= policy_t::evaluate(data[index], hints[index], x);
        }
    }

    _mm_lfence();
    auto const end_cycles = __rdtscp(&aux);
    _mm_lfence();

    do_not_optimize(result);
    auto const sample_count = rotations * data.size();
    return static_cast<float_t>(end_cycles - start_cycles) / static_cast<float_t>(sample_count);
}

template <typename policy_t>
auto benchmark_median(std::vector<spline_data_t> const& data, std::vector<std::size_t> const& order, motion_t motion,
    std::size_t rotations, std::size_t trials) -> float_t
{
    auto results = std::vector<float_t>{};
    results.reserve(trials);
    for (auto trial = std::size_t{0}; trial < trials; ++trial)
        results.push_back(benchmark<policy_t>(data, order, motion, rotations));
    std::ranges::sort(results);
    return results[results.size() / 2];
}

struct hit_rates_t
{
    double segment;
    double leaf;
};

auto hit_rates(std::size_t instance_count, motion_t motion, std::size_t rotations) -> hit_rates_t
{
    auto segment_hits = std::size_t{};
    auto leaf_hits = std::size_t{};
    auto const sample_count = instance_count * rotations;

    for (auto rotation = std::size_t{1}; rotation <= rotations; ++rotation)
    {
        for (auto index = std::size_t{0}; index < instance_count; ++index)
        {
            auto const previous = segment_for(motion, index, rotation - 1);
            auto const current = segment_for(motion, index, rotation);
            segment_hits += previous == current;
            leaf_hits
                += previous / benchmark_locator_t::branching_factor == current / benchmark_locator_t::branching_factor;
        }
    }

    auto const denominator = static_cast<double>(sample_count);
    return {
        .segment = 100.0 * static_cast<double>(segment_hits) / denominator,
        .leaf = 100.0 * static_cast<double>(leaf_hits) / denominator,
    };
}

template <typename policy_t>
auto evaluate_trace_sample(spline_data_t const& data, typename policy_t::hint_t& hint, x_t x,
    static_prefetcher_t const& prefetcher) noexcept -> uint64_t
{
    prefetcher.prefetch(&data.tangent);
    policy_t::prefetch(data, hint, prefetcher);

    if (x >= data.locator.x_max())
    {
        return data.tangent[0] ^ data.tangent[1] ^ data.tangent[2] ^ data.tangent[3] ^ int_cast<uint64_t>(x.value);
    }

    return policy_t::evaluate(data, hint, x);
}

auto make_trace_offsets(std::size_t instance_count, std::vector<uint64_t> const& epochs, std::size_t rotations)
    -> std::vector<std::size_t>
{
    auto valid_offsets = std::vector<std::size_t>{};
    valid_offsets.reserve(epochs.size());
    for (auto offset = std::size_t{0}; offset + rotations < epochs.size(); ++offset)
        if (epochs[offset] == epochs[offset + rotations]) valid_offsets.push_back(offset);

    if (valid_offsets.empty()) return {};

    auto result = std::vector<std::size_t>(instance_count);
    auto rng = std::mt19937_64{0x747261636568696eull};
    auto distribution = std::uniform_int_distribution<std::size_t>{0, valid_offsets.size() - 1};
    for (auto& offset : result) offset = valid_offsets[distribution(rng)];
    return result;
}

template <typename policy_t>
auto warmup_trace(std::vector<spline_data_t> const& data, std::vector<typename policy_t::hint_t>& hints,
    std::vector<std::size_t> const& order, std::vector<std::size_t> const& offsets,
    std::vector<x_t> const& trace) noexcept -> uint64_t
{
    auto result = uint64_t{};
    auto const prefetcher = static_prefetcher_t{};
    for (auto const index : order)
        result ^= evaluate_trace_sample<policy_t>(data[index], hints[index], trace[offsets[index]], prefetcher);
    return result;
}

template <typename policy_t>
auto benchmark_trace(std::vector<spline_data_t> const& data, std::vector<std::size_t> const& order,
    std::vector<std::size_t> const& offsets, std::vector<x_t> const& trace, std::size_t rotations) -> float_t
{
    auto hints = std::vector<typename policy_t::hint_t>(data.size());
    do_not_optimize(warmup_trace<policy_t>(data, hints, order, offsets, trace));

    auto const prefetcher = static_prefetcher_t{};
    auto result = uint64_t{};
    auto aux = uint32_t{};

    _mm_lfence();
    auto const start_cycles = __rdtsc();
    _mm_lfence();

    for (auto rotation = std::size_t{1}; rotation <= rotations; ++rotation)
        for (auto const index : order)
            result ^= evaluate_trace_sample<policy_t>(
                data[index], hints[index], trace[offsets[index] + rotation], prefetcher);

    _mm_lfence();
    auto const end_cycles = __rdtscp(&aux);
    _mm_lfence();

    do_not_optimize(result);
    auto const sample_count = rotations * data.size();
    return static_cast<float_t>(end_cycles - start_cycles) / static_cast<float_t>(sample_count);
}

template <typename policy_t>
auto benchmark_trace_median(std::vector<spline_data_t> const& data, std::vector<std::size_t> const& order,
    std::vector<std::size_t> const& offsets, std::vector<x_t> const& trace, std::size_t rotations, std::size_t trials)
    -> float_t
{
    auto results = std::vector<float_t>{};
    results.reserve(trials);
    for (auto trial = std::size_t{0}; trial < trials; ++trial)
        results.push_back(benchmark_trace<policy_t>(data, order, offsets, trace, rotations));
    std::ranges::sort(results);
    return results[results.size() / 2];
}

template <typename entry_policy_t, typename spline_policy_t,
    typename spline_extra_prefetch_policy_t = tangent_prefetch_policy>
auto warmup_scheduled_trace_entry(std::vector<pipeline_data_t>& data, std::vector<std::size_t> const& order,
    std::vector<std::size_t> const& offsets, std::vector<x_t> const& trace) noexcept -> uint64_t
{
    auto result = uint64_t{};
    auto const prefetcher = static_prefetcher_t{};
    for (auto const index : order)
        result ^= evaluate_scheduled_entry_with_x<entry_policy_t, spline_policy_t, spline_extra_prefetch_policy_t>(
            data[index], index, 0, trace[offsets[index]], prefetcher);
    return result;
}

template <typename entry_policy_t, typename spline_policy_t,
    typename spline_extra_prefetch_policy_t = tangent_prefetch_policy>
auto benchmark_scheduled_trace_entry(std::vector<pipeline_data_t>& data, std::vector<std::size_t> const& order,
    std::vector<std::size_t> const& offsets, std::vector<x_t> const& trace, std::size_t rotations) -> float_t
{
    for (auto& pipeline : data) pipeline.state = {};
    do_not_optimize(warmup_scheduled_trace_entry<entry_policy_t, spline_policy_t, spline_extra_prefetch_policy_t>(
        data, order, offsets, trace));

    auto const prefetcher = static_prefetcher_t{};
    auto result = uint64_t{};
    auto aux = uint32_t{};

    _mm_lfence();
    auto const start_cycles = __rdtsc();
    _mm_lfence();

    for (auto rotation = std::size_t{1}; rotation <= rotations; ++rotation)
        for (auto const index : order)
            result ^= evaluate_scheduled_entry_with_x<entry_policy_t, spline_policy_t, spline_extra_prefetch_policy_t>(
                data[index], index, rotation, trace[offsets[index] + rotation], prefetcher);

    _mm_lfence();
    auto const end_cycles = __rdtscp(&aux);
    _mm_lfence();

    do_not_optimize(result);
    auto const sample_count = rotations * data.size();
    return static_cast<float_t>(end_cycles - start_cycles) / static_cast<float_t>(sample_count);
}

template <typename entry_policy_t, typename spline_policy_t,
    typename spline_extra_prefetch_policy_t = tangent_prefetch_policy>
auto benchmark_scheduled_trace_entry_median(std::vector<pipeline_data_t>& data, std::vector<std::size_t> const& order,
    std::vector<std::size_t> const& offsets, std::vector<x_t> const& trace, std::size_t rotations, std::size_t trials)
    -> float_t
{
    auto results = std::vector<float_t>{};
    results.reserve(trials);
    for (auto trial = std::size_t{0}; trial < trials; ++trial)
        results.push_back(
            benchmark_scheduled_trace_entry<entry_policy_t, spline_policy_t, spline_extra_prefetch_policy_t>(
                data, order, offsets, trace, rotations));
    std::ranges::sort(results);
    return results[results.size() / 2];
}

struct trace_hit_rates_t
{
    double segment{};
    double leaf{};
    double tangent{};
    std::size_t tangent_samples{};
};

auto trace_hit_rates(benchmark_locator_t const& locator, std::vector<std::size_t> const& offsets,
    std::vector<x_t> const& trace, std::size_t rotations) -> trace_hit_rates_t
{
    auto segment_hits = std::size_t{};
    auto leaf_hits = std::size_t{};
    auto located_samples = std::size_t{};
    auto tangent_samples = std::size_t{};

    for (auto const offset : offsets)
    {
        auto previous_index = int_t{0};
        auto const initial_x = trace[offset];
        if (initial_x < locator.x_max()) previous_index = locator.locate(initial_x).index;

        for (auto rotation = std::size_t{1}; rotation <= rotations; ++rotation)
        {
            auto const x = trace[offset + rotation];
            if (x >= locator.x_max())
            {
                ++tangent_samples;
                continue;
            }

            auto const index = locator.locate(x).index;
            segment_hits += index == previous_index;
            leaf_hits += index / benchmark_locator_t::branching_factor
                == previous_index / benchmark_locator_t::branching_factor;
            previous_index = index;
            ++located_samples;
        }
    }

    auto const total_samples = offsets.size() * rotations;
    return {
        .segment
        = located_samples == 0 ? 0.0 : 100.0 * static_cast<double>(segment_hits) / static_cast<double>(located_samples),
        .leaf
        = located_samples == 0 ? 0.0 : 100.0 * static_cast<double>(leaf_hits) / static_cast<double>(located_samples),
        .tangent
        = total_samples == 0 ? 0.0 : 100.0 * static_cast<double>(tangent_samples) / static_cast<double>(total_samples),
        .tangent_samples = tangent_samples,
    };
}

auto parse_size(char const* text, std::size_t& value) -> bool
{
    auto const view = std::string_view{text};
    auto const result = std::from_chars(view.data(), view.data() + view.size(), value);
    return result.ec == std::errc{} && result.ptr == view.data() + view.size() && value > 0;
}

auto run(std::size_t working_set_mib, std::size_t rotations, std::size_t trials) -> int
{
    if (!verify_locator())
    {
        std::cerr << "benchmark locator does not match production locator\n";
        return 1;
    }

    constexpr auto bytes_per_mib = std::size_t{1024 * 1024};
    auto const requested_bytes = working_set_mib * bytes_per_mib;
    constexpr auto motions = std::array{
        motion_t::stationary,
        motion_t::segment_walk,
        motion_t::leaf_walk,
        motion_t::random,
    };

    {
        auto const instance_count = max(std::size_t{1}, requested_bytes / sizeof(spline_data_t));
        auto const actual_bytes = instance_count * sizeof(spline_data_t);

        std::cout << "spline data: " << sizeof(spline_data_t) << " bytes\n";
        std::cout << "instances:   " << instance_count << "\n";
        std::cout << "working set: " << std::fixed << std::setprecision(1)
                  << static_cast<double>(actual_bytes) / static_cast<double>(bytes_per_mib) << " MiB\n";
        std::cout << "rotations:   " << rotations << "\n";
        std::cout << "trials:      " << trials << " (median reported)\n\n";

        auto const prototype = make_prototype();
        auto data = std::vector<spline_data_t>(instance_count, prototype);
        auto order = std::vector<std::size_t>(instance_count);
        std::iota(order.begin(), order.end(), std::size_t{0});
        auto rng = std::mt19937_64{0x637572766573ull};
        std::shuffle(order.begin(), order.end(), rng);

        std::cout << std::left << std::setw(18) << "motion" << std::right << std::setw(10) << "seg-hit%"
                  << std::setw(10) << "leaf-hit%" << std::setw(18) << baseline_policy_t::name << std::setw(18)
                  << segment_hint_policy_t::name << std::setw(18) << leaf_hint_policy::name << std::setw(18)
                  << leaf_hint_root_policy::name << std::setw(18) << leaf_hint_top2_policy::name << '\n';

        for (auto const motion : motions)
        {
            auto const baseline = benchmark_median<baseline_policy_t>(data, order, motion, rotations, trials);
            auto const segment = benchmark_median<segment_hint_policy_t>(data, order, motion, rotations, trials);
            auto const leaf = benchmark_median<leaf_hint_policy>(data, order, motion, rotations, trials);
            auto const leaf_root = benchmark_median<leaf_hint_root_policy>(data, order, motion, rotations, trials);
            auto const leaf_top2 = benchmark_median<leaf_hint_top2_policy>(data, order, motion, rotations, trials);

            auto const hits = hit_rates(instance_count, motion, rotations);
            std::cout << std::left << std::setw(18) << motion_name(motion) << std::right << std::fixed
                      << std::setprecision(1) << std::setw(10) << hits.segment << std::setw(10) << hits.leaf
                      << std::setprecision(2) << std::setw(18) << baseline << std::setw(18) << segment << std::setw(18)
                      << leaf << std::setw(18) << leaf_root << std::setw(18) << leaf_top2 << '\n';
        }

        std::cout << "\ncycles/sample; rotating order is fixed and randomized\n";

        std::cout << "\ncandidate spline policies\n";
        std::cout << std::left << std::setw(18) << "motion" << std::right << std::setw(18) << full_hint_policy_t::name
                  << std::setw(18) << leaf_hint_policy::name << std::setw(18) << leaf_lines_policy_t::name
                  << std::setw(18) << adaptive_leaf_1bit_policy::name << std::setw(18)
                  << adaptive_leaf_2bit_policy::name << '\n';

        for (auto const motion : motions)
        {
            auto const full = benchmark_median<full_hint_policy_t>(data, order, motion, rotations, trials);
            auto const leaf = benchmark_median<leaf_hint_policy>(data, order, motion, rotations, trials);
            auto const leaf_lines = benchmark_median<leaf_lines_policy_t>(data, order, motion, rotations, trials);
            auto const adaptive_1bit
                = benchmark_median<adaptive_leaf_1bit_policy>(data, order, motion, rotations, trials);
            auto const adaptive_2bit
                = benchmark_median<adaptive_leaf_2bit_policy>(data, order, motion, rotations, trials);

            std::cout << std::left << std::setw(18) << motion_name(motion) << std::right << std::fixed
                      << std::setprecision(2) << std::setw(18) << full << std::setw(18) << leaf << std::setw(18)
                      << leaf_lines << std::setw(18) << adaptive_1bit << std::setw(18) << adaptive_2bit << '\n';
        }

        std::cout << "\nproduction-full refreshes the same 24-byte leaf hint as the runtime full lookup\n";
    }

    {
        auto const instance_count = max(std::size_t{1}, requested_bytes / sizeof(pipeline_data_t));
        auto const actual_bytes = instance_count * sizeof(pipeline_data_t);

        std::cout << "\nentry pipeline data: " << sizeof(pipeline_data_t) << " bytes\n";
        std::cout << "instances:           " << instance_count << "\n";
        std::cout << "working set:         " << std::fixed << std::setprecision(1)
                  << static_cast<double>(actual_bytes) / static_cast<double>(bytes_per_mib) << " MiB\n";
        std::cout << "spline policy:       " << leaf_hint_root_policy::name << "\n";
        std::cout << "rotations:           " << rotations << "\n";
        std::cout << "trials:              " << trials << " (median reported)\n\n";

        auto const prototype = make_pipeline_prototype();
        auto data = std::vector<pipeline_data_t>(instance_count, prototype);
        auto order = std::vector<std::size_t>(instance_count);
        std::iota(order.begin(), order.end(), std::size_t{0});
        auto rng = std::mt19937_64{0x637572766573ull};
        std::shuffle(order.begin(), order.end(), rng);

        std::cout << std::left << std::setw(18) << "motion" << std::right << std::setw(18)
                  << no_entry_prefetch_policy::name << std::setw(18) << control_prefetch_policy::name << std::setw(18)
                  << state_prefetch_policy::name << std::setw(18) << control_state_prefetch_policy::name
                  << std::setw(18) << state_control_prefetch_policy::name << '\n';

        for (auto const motion : motions)
        {
            auto const none = benchmark_entry_median<no_entry_prefetch_policy>(data, order, motion, rotations, trials);
            auto const control
                = benchmark_entry_median<control_prefetch_policy>(data, order, motion, rotations, trials);
            auto const state = benchmark_entry_median<state_prefetch_policy>(data, order, motion, rotations, trials);
            auto const both
                = benchmark_entry_median<control_state_prefetch_policy>(data, order, motion, rotations, trials);
            auto const state_control
                = benchmark_entry_median<state_control_prefetch_policy>(data, order, motion, rotations, trials);

            std::cout << std::left << std::setw(18) << motion_name(motion) << std::right << std::fixed
                      << std::setprecision(2) << std::setw(18) << none << std::setw(18) << control << std::setw(18)
                      << state << std::setw(18) << both << std::setw(18) << state_control << '\n';
        }

        std::cout << "\ncycles/sample; early prefetch precedes framing-derived work and synchronization check\n";

        std::cout << "\nstaged entry schedule\n";
        std::cout << std::left << std::setw(18) << "lookup" << std::setw(18) << "motion" << std::right << std::setw(18)
                  << no_entry_prefetch_policy::name << std::setw(18) << control_prefetch_policy::name << std::setw(18)
                  << control_state_prefetch_policy::name << std::setw(18) << staged_prefetch_policy::name << '\n';

        auto print_staged = [&]<typename spline_policy_t>(std::string_view lookup_name) {
            for (auto const motion : motions)
            {
                auto const none = benchmark_scheduled_entry_median<no_entry_prefetch_policy, spline_policy_t>(
                    data, order, motion, rotations, trials);
                auto const control = benchmark_scheduled_entry_median<control_prefetch_policy, spline_policy_t>(
                    data, order, motion, rotations, trials);
                auto const both = benchmark_scheduled_entry_median<control_state_prefetch_policy, spline_policy_t>(
                    data, order, motion, rotations, trials);
                auto const staged = benchmark_scheduled_entry_median<staged_prefetch_policy, spline_policy_t>(
                    data, order, motion, rotations, trials);

                std::cout << std::left << std::setw(18) << lookup_name << std::setw(18) << motion_name(motion)
                          << std::right << std::fixed << std::setprecision(2) << std::setw(18) << none << std::setw(18)
                          << control << std::setw(18) << both << std::setw(18) << staged << '\n';
            }
        };

        print_staged.template operator()<full_hint_policy_t>("production-full");
        print_staged.template operator()<leaf_hint_policy>("leaf-range");

        std::cout << "\nstate prefetch in staged mode is issued after synchronization and before real frame/report "
                     "inspection\n";
    }

    return 0;
}

auto run_capture(char const* capture_path, char const* config_path, std::size_t working_set_mib, std::size_t rotations,
    std::size_t trials) -> int
{
    std::cerr << "warning: capture format v1 does not store live input-core num_vals; split detection uses the legacy "
                 "SYN_REPORT.value hint and is not production-equivalent\n";

    auto config = replay_configuration_t{};
    if (!load_replay_configuration(config_path, config)) return 1;

    auto trace = replay_trace_t{};
    if (!load_replay_trace(capture_path, config, trace)) return 1;
    if (trace.speed.size() <= rotations)
    {
        std::cerr << capture_path << ": capture has too few spline-input samples for " << rotations << " rotations\n";
        return 1;
    }

    auto const& production_locator = config.spline.segment_locator;
    auto const prototype = make_prototype(production_locator);
    if (!prototype)
    {
        std::cerr << "failed to recover production locator breakpoints for replay benchmark\n";
        return 1;
    }
    if (!verify_locator(production_locator, prototype->locator))
    {
        std::cerr << "recovered replay locator does not match production locator\n";
        return 1;
    }

    constexpr auto bytes_per_mib = std::size_t{1024 * 1024};
    auto const requested_bytes = working_set_mib * bytes_per_mib;
    auto const instance_count = max(std::size_t{1}, requested_bytes / sizeof(spline_data_t));
    auto const actual_bytes = instance_count * sizeof(spline_data_t);

    auto data = std::vector<spline_data_t>(instance_count, *prototype);
    auto order = std::vector<std::size_t>(instance_count);
    std::iota(order.begin(), order.end(), std::size_t{0});
    auto rng = std::mt19937_64{0x637572766573ull};
    std::shuffle(order.begin(), order.end(), rng);
    auto const offsets = make_trace_offsets(instance_count, trace.epoch, rotations);
    if (offsets.empty())
    {
        std::cerr << capture_path << ": no contiguous replay window spans " << rotations << " rotations\n";
        return 1;
    }
    auto const hits = trace_hit_rates(prototype->locator, offsets, trace.speed, rotations);

    auto const baseline
        = benchmark_trace_median<baseline_policy_t>(data, order, offsets, trace.speed, rotations, trials);
    auto const segment
        = benchmark_trace_median<segment_hint_policy_t>(data, order, offsets, trace.speed, rotations, trials);
    auto const leaf = benchmark_trace_median<leaf_hint_policy>(data, order, offsets, trace.speed, rotations, trials);
    auto const leaf_root
        = benchmark_trace_median<leaf_hint_root_policy>(data, order, offsets, trace.speed, rotations, trials);
    auto const leaf_top2
        = benchmark_trace_median<leaf_hint_top2_policy>(data, order, offsets, trace.speed, rotations, trials);
    auto const full_hint
        = benchmark_trace_median<full_hint_policy_t>(data, order, offsets, trace.speed, rotations, trials);
    auto const leaf_lines
        = benchmark_trace_median<leaf_lines_policy_t>(data, order, offsets, trace.speed, rotations, trials);
    auto const adaptive_1bit
        = benchmark_trace_median<adaptive_leaf_1bit_policy>(data, order, offsets, trace.speed, rotations, trials);
    auto const adaptive_2bit
        = benchmark_trace_median<adaptive_leaf_2bit_policy>(data, order, offsets, trace.speed, rotations, trials);

    std::cout << "capture replay\n";
    std::cout << "  frames:                 " << trace.frame_count << '\n';
    std::cout << "  spline-input samples:   " << trace.speed.size() << '\n';
    std::cout << "  invalid reports:        " << trace.invalid_report_count << '\n';
    std::cout << "  split callbacks:        " << trace.split_frame_count << '\n';
    std::cout << "  resynchronizations:     " << trace.resynchronization_count << '\n';
    std::cout << "  invalid timestamps:     " << trace.invalid_timestamp_count << '\n';
    std::cout << "  velocity out of range:  " << trace.velocity_out_of_range_count << '\n';
    std::cout << "  dpi:                    " << config.dpi << '\n';
    std::cout << "  filter half-life:       " << std::fixed << std::setprecision(3) << config.half_life_ms << " ms\n";
    std::cout << "  spline segments:        " << production_locator.segment_count() << '\n';
    std::cout << "  tangent samples:        " << std::setprecision(2) << hits.tangent << "% (" << hits.tangent_samples
              << ")\n\n";

    std::cout << "spline data: " << sizeof(spline_data_t) << " bytes\n";
    std::cout << "instances:   " << instance_count << '\n';
    std::cout << "working set: " << std::fixed << std::setprecision(1)
              << static_cast<double>(actual_bytes) / static_cast<double>(bytes_per_mib) << " MiB\n";
    std::cout << "rotations:   " << rotations << '\n';
    std::cout << "trials:      " << trials << " (median reported)\n\n";

    std::cout << std::left << std::setw(18) << "motion" << std::right << std::setw(10) << "seg-hit%" << std::setw(10)
              << "leaf-hit%" << std::setw(18) << baseline_policy_t::name << std::setw(18) << segment_hint_policy_t::name
              << std::setw(18) << leaf_hint_policy::name << std::setw(18) << leaf_hint_root_policy::name
              << std::setw(18) << leaf_hint_top2_policy::name << '\n';

    std::cout << std::left << std::setw(18) << "capture" << std::right << std::fixed << std::setprecision(1)
              << std::setw(10) << hits.segment << std::setw(10) << hits.leaf << std::setprecision(2) << std::setw(18)
              << baseline << std::setw(18) << segment << std::setw(18) << leaf << std::setw(18) << leaf_root
              << std::setw(18) << leaf_top2 << '\n';

    std::cout << "\ncycles/sample; each rotating spline instance replays a contiguous capture window\n";

    std::cout << "\ncandidate spline policies\n";
    std::cout << std::left << std::setw(18) << "motion" << std::right << std::setw(18) << full_hint_policy_t::name
              << std::setw(18) << leaf_hint_policy::name << std::setw(18) << leaf_lines_policy_t::name << std::setw(18)
              << adaptive_leaf_1bit_policy::name << std::setw(18) << adaptive_leaf_2bit_policy::name << '\n';
    std::cout << std::left << std::setw(18) << "capture" << std::right << std::fixed << std::setprecision(2)
              << std::setw(18) << full_hint << std::setw(18) << leaf << std::setw(18) << leaf_lines << std::setw(18)
              << adaptive_1bit << std::setw(18) << adaptive_2bit << '\n';
    std::cout << "\nproduction-full refreshes the same 24-byte leaf hint as the runtime full lookup\n";

    auto const pipeline_prototype = make_pipeline_prototype(production_locator);
    if (!pipeline_prototype)
    {
        std::cerr << "failed to construct production-topology entry prototype for replay benchmark\n";
        return 1;
    }

    auto const entry_instance_count = max(std::size_t{1}, requested_bytes / sizeof(pipeline_data_t));
    auto const entry_actual_bytes = entry_instance_count * sizeof(pipeline_data_t);
    auto entry_data = std::vector<pipeline_data_t>(entry_instance_count, *pipeline_prototype);
    auto entry_order = std::vector<std::size_t>(entry_instance_count);
    std::iota(entry_order.begin(), entry_order.end(), std::size_t{0});
    std::shuffle(entry_order.begin(), entry_order.end(), rng);
    auto const entry_offsets = make_trace_offsets(entry_instance_count, trace.epoch, rotations);
    if (entry_offsets.empty())
    {
        std::cerr << capture_path << ": no contiguous replay window spans " << rotations
                  << " rotations for entry benchmark\n";
        return 1;
    }

    std::cout << "\ncapture-driven staged entry schedule\n";
    std::cout << "entry pipeline data: " << sizeof(pipeline_data_t) << " bytes\n";
    std::cout << "instances:           " << entry_instance_count << '\n';
    std::cout << "working set:         " << std::fixed << std::setprecision(1)
              << static_cast<double>(entry_actual_bytes) / static_cast<double>(bytes_per_mib) << " MiB\n\n";
    std::cout << std::left << std::setw(18) << "lookup" << std::right << std::setw(18) << no_entry_prefetch_policy::name
              << std::setw(18) << control_prefetch_policy::name << std::setw(18) << control_state_prefetch_policy::name
              << std::setw(18) << staged_prefetch_policy::name << '\n';

    auto print_capture_staged = [&]<typename spline_policy_t>(std::string_view lookup_name) {
        auto const none = benchmark_scheduled_trace_entry_median<no_entry_prefetch_policy, spline_policy_t>(
            entry_data, entry_order, entry_offsets, trace.speed, rotations, trials);
        auto const control = benchmark_scheduled_trace_entry_median<control_prefetch_policy, spline_policy_t>(
            entry_data, entry_order, entry_offsets, trace.speed, rotations, trials);
        auto const both = benchmark_scheduled_trace_entry_median<control_state_prefetch_policy, spline_policy_t>(
            entry_data, entry_order, entry_offsets, trace.speed, rotations, trials);
        auto const staged = benchmark_scheduled_trace_entry_median<staged_prefetch_policy, spline_policy_t>(
            entry_data, entry_order, entry_offsets, trace.speed, rotations, trials);

        std::cout << std::left << std::setw(18) << lookup_name << std::right << std::fixed << std::setprecision(2)
                  << std::setw(18) << none << std::setw(18) << control << std::setw(18) << both << std::setw(18)
                  << staged << '\n';
    };

    print_capture_staged.template operator()<full_hint_policy_t>("production-full");
    print_capture_staged.template operator()<leaf_hint_policy>("leaf-range");
    std::cout << "\nentry schedule uses replayed spline x with synthetic normal framing and real frame/report "
                 "inspection\n";

    std::cout << "\nspline extra-prefetch request\n";
    std::cout << std::left << std::setw(18) << "entry" << std::setw(18) << "lookup" << std::right << std::setw(18)
              << tangent_prefetch_policy::name << std::setw(18) << header_prefetch_policy::name << std::setw(18)
              << no_spline_extra_prefetch_policy::name << '\n';

    auto print_capture_extra = [&]<typename entry_policy_t, typename spline_policy_t>(
                                   std::string_view entry_name, std::string_view lookup_name) {
        auto const tangent
            = benchmark_scheduled_trace_entry_median<entry_policy_t, spline_policy_t, tangent_prefetch_policy>(
                entry_data, entry_order, entry_offsets, trace.speed, rotations, trials);
        auto const header
            = benchmark_scheduled_trace_entry_median<entry_policy_t, spline_policy_t, header_prefetch_policy>(
                entry_data, entry_order, entry_offsets, trace.speed, rotations, trials);
        auto const none
            = benchmark_scheduled_trace_entry_median<entry_policy_t, spline_policy_t, no_spline_extra_prefetch_policy>(
                entry_data, entry_order, entry_offsets, trace.speed, rotations, trials);

        std::cout << std::left << std::setw(18) << entry_name << std::setw(18) << lookup_name << std::right
                  << std::fixed << std::setprecision(2) << std::setw(18) << tangent << std::setw(18) << header
                  << std::setw(18) << none << '\n';
    };

    print_capture_extra.template operator()<control_state_prefetch_policy, full_hint_policy_t>(
        "control+state", "production-full");
    print_capture_extra.template operator()<staged_prefetch_policy, full_hint_policy_t>("staged", "production-full");
    print_capture_extra.template operator()<control_state_prefetch_policy, leaf_hint_policy>(
        "control+state", "leaf-range");
    print_capture_extra.template operator()<staged_prefetch_policy, leaf_hint_policy>("staged", "leaf-range");
    std::cout << "\nheader is the spline locator/header line containing x_max; tangent is the current production "
                 "request\n";
    return 0;
}

} // namespace
} // namespace crv

auto main(int argc, char** argv) -> int
{
    auto working_set_mib = std::size_t{256};
    auto rotations = std::size_t{4};
    auto trials = std::size_t{5};

    if (argc > 1 && std::string_view{argv[1]} == "--capture")
    {
        if (argc < 4 || argc > 7)
        {
            std::cerr << "usage: performance_test_spline_hint --capture CAPTURE_FILE CONFIG_FILE "
                         "[working-set-mib] [rotations] [odd-trials]\n";
            return 2;
        }

        if (argc > 4 && !crv::parse_size(argv[4], working_set_mib))
        {
            std::cerr << "invalid working-set MiB\n";
            return 2;
        }
        if (argc > 5 && !crv::parse_size(argv[5], rotations))
        {
            std::cerr << "invalid rotation count\n";
            return 2;
        }
        if (argc > 6 && !crv::parse_size(argv[6], trials))
        {
            std::cerr << "invalid trial count\n";
            return 2;
        }
        if ((trials & 1u) == 0)
        {
            std::cerr << "trial count must be odd\n";
            return 2;
        }

        return crv::run_capture(argv[2], argv[3], working_set_mib, rotations, trials);
    }

    if (argc > 1 && !crv::parse_size(argv[1], working_set_mib))
    {
        std::cerr << "invalid working-set MiB\n";
        return 2;
    }
    if (argc > 2 && !crv::parse_size(argv[2], rotations))
    {
        std::cerr << "invalid rotation count\n";
        return 2;
    }
    if (argc > 3 && !crv::parse_size(argv[3], trials))
    {
        std::cerr << "invalid trial count\n";
        return 2;
    }
    if ((trials & 1u) == 0)
    {
        std::cerr << "trial count must be odd\n";
        return 2;
    }
    if (argc > 4)
    {
        std::cerr << "usage: performance_test_spline_hint [working-set-mib] [rotations] [odd-trials]\n";
        return 2;
    }

    return crv::run(working_set_mib, rotations, trials);
}
