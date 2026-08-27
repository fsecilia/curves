// SPDX-License-Identifier: MIT

/// \file
/// \brief rotating-cache spline locator hint benchmark
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/prefetcher.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/segment_locator.hpp>
#include <crv/test/performance/performance.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string_view>
#include <vector>

namespace crv {
namespace {

using x_t = spline::prod_pipeline_config_t::x_t;
constexpr auto depth_max = int_t{4};
using production_locator_t = spline::segment_locator_t<x_t, depth_max>;

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

static_assert(sizeof(index_hint_t) == 8);
static_assert(sizeof(segment_hint_t) == 24);
static_assert(sizeof(leaf_hint_t) == 24);

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
    std::array<uint64_t, 7> words{};
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
static_assert(sizeof(config_t) == 56);
static_assert(sizeof(control_t) == 64);
static_assert(offsetof(control_t, framing) == 56);
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

template <bool prefetch_control, bool prefetch_state> struct entry_prefetch_policy_t
{
    static constexpr auto name = !prefetch_control && !prefetch_state ? std::string_view{"none"}
        : prefetch_control && !prefetch_state                         ? std::string_view{"control"}
        : !prefetch_control && prefetch_state                         ? std::string_view{"state"}
                                                                      : std::string_view{"control+state"};

    static auto prefetch(pipeline_data_t const& pipeline, static_prefetcher_t const& prefetcher) noexcept -> void
    {
        if constexpr (prefetch_control) prefetcher.prefetch(&pipeline.control);
        if constexpr (prefetch_state) prefetcher.prefetch(&pipeline.state);
    }
};

using no_entry_prefetch_policy = entry_prefetch_policy_t<false, false>;
using control_prefetch_policy = entry_prefetch_policy_t<true, false>;
using state_prefetch_policy = entry_prefetch_policy_t<false, true>;
using control_state_prefetch_policy = entry_prefetch_policy_t<true, true>;

struct state_control_prefetch_policy
{
    static constexpr auto name = std::string_view{"state+control"};

    static auto prefetch(pipeline_data_t const& pipeline, static_prefetcher_t const& prefetcher) noexcept -> void
    {
        prefetcher.prefetch(&pipeline.state);
        prefetcher.prefetch(&pipeline.control);
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

auto prefetch_segment_neighbors(
    spline_data_t const& data, int_t segment_index, static_prefetcher_t const& prefetcher) noexcept -> void
{
    auto const base_address = reinterpret_cast<std::uintptr_t>(data.segments.data());
    auto const offset = sizeof(segment_t);
    prefetcher.prefetch(reinterpret_cast<void const*>(base_address + (segment_index - 1) * offset));
    prefetcher.prefetch(reinterpret_cast<void const*>(base_address + (segment_index + 1) * offset));
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

auto verify_locator() -> bool
{
    auto const keys = make_sorted_keys();
    auto const production = production_locator_t{keys, x_max, production_locator_t::max_segment_count};
    auto const benchmark = benchmark_locator_t{keys, x_max};

    for (auto raw = int_t{0}; raw < benchmark_locator_t::max_segment_count * segment_width; ++raw)
    {
        auto const x = x_t{raw};
        auto const expected = production.locate(x);
        auto const actual = benchmark.locate(x);
        auto const extended = benchmark.locate_with_segment_range(x);
        auto const leaf_extended = benchmark.locate_with_leaf_range(x);
        if (actual.index != expected.index || actual.origin != expected.origin) return false;
        if (extended.index != expected.index || extended.origin != expected.origin) return false;
        if (!(extended.origin <= x && x < extended.end)) return false;
        if (!(extended.leaf_origin <= x && x < extended.leaf_end)) return false;
        if (extended.index / benchmark_locator_t::branching_factor != extended.leaf_index) return false;
        if (leaf_extended.index != expected.index || leaf_extended.origin != expected.origin) return false;
        if (!(leaf_extended.leaf_origin <= x && x < leaf_extended.leaf_end)) return false;
        if (leaf_extended.index / benchmark_locator_t::branching_factor != leaf_extended.leaf_index) return false;
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
    }

    return 0;
}

} // namespace
} // namespace crv

auto main(int argc, char** argv) -> int
{
    auto working_set_mib = std::size_t{256};
    auto rotations = std::size_t{4};
    auto trials = std::size_t{5};

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
