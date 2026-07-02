// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "overloaded.hpp"
#include <crv/test/test.hpp>
#include <variant>

namespace crv {
namespace {

constexpr auto const key_int = int_t{7};
constexpr auto const key_float = float_t{7.5};

// exact match
constexpr auto pick = overloaded_t{[](int_t) { return 1; }, [](float_t) { return 2; }};
static_assert(pick(key_int) == 1);
static_assert(pick(key_float) == 2);

// specific overload beats generic fallback
constexpr auto generic_auto = overloaded_t{[](int_t) { return 1; }, [](auto) { return 0; }};
static_assert(generic_auto(key_int) == 1);
static_assert(generic_auto(key_float) == 0);

// specific overload beats match-all fallback
constexpr auto generic_auto_forwarding_ref = overloaded_t{[](int_t) { return 1; }, [](auto&&) { return 0; }};
static_assert(generic_auto_forwarding_ref(key_int) == 1);
static_assert(generic_auto_forwarding_ref(key_float) == 0);

// actual use case
static_assert(std::visit(pick, std::variant<int_t, float_t>{key_int}) == 1);
static_assert(std::visit(pick, std::variant<int_t, float_t>{key_float}) == 2);

// state and mutation flow through the call operators
struct mutation_test_t
{
    bool matched_int = false;
    bool matched_float = false;

    constexpr auto create_matcher() noexcept -> auto
    {
        return overloaded_t{[&](int_t) { matched_int = true; }, [&](float_t) { matched_float = true; }};
    }

    static constexpr auto match_int() noexcept -> bool
    {
        auto instance = mutation_test_t{};
        instance.create_matcher()(key_int);
        return instance.matched_int && !instance.matched_float;
    }

    static constexpr auto match_float() noexcept -> bool
    {
        auto instance = mutation_test_t{};
        instance.create_matcher()(key_float);
        return !instance.matched_int && instance.matched_float;
    }
};
static_assert(mutation_test_t::match_int());
static_assert(mutation_test_t::match_float());

// test move only construction
struct move_only_callable_t
{
    move_only_callable_t() = default;
    move_only_callable_t(move_only_callable_t const&) = delete;
    move_only_callable_t(move_only_callable_t&&) = default;

    constexpr auto operator()(char) const noexcept -> int_t { return 3; }
};
static_assert(overloaded_t{move_only_callable_t{}, [](float_t) { return 0; }}('x') == 3);

} // namespace
} // namespace crv
