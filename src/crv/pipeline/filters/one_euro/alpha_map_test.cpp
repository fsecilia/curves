// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "alpha_map.hpp"
#include <crv/test/test.hpp>

namespace crv::pipeline::filters::one_euro {
namespace {

using cutoff_step_t = fixed_t<uint64_t, 58>;
using smoothing_factor_t = fixed_t<uint64_t, 64>;
constexpr auto sut = alpha_map_t<smoothing_factor_t>{};

static_assert(sut(cutoff_step_t{}) == smoothing_factor_t{});
static_assert(sut(cutoff_step_t{1}) == smoothing_factor_t::literal(uint64_t{1} << 63));
static_assert(sut(cutoff_step_t{2}).value == 0xAAAAAAAAAAAAAAAAULL);
static_assert(sut(cutoff_step_t{31}) == smoothing_factor_t::literal(uint64_t{31} << 59));

constexpr auto max_safe = max<cutoff_step_t>() - cutoff_step_t{1};
static_assert(sut(max_safe) > smoothing_factor_t{});
static_assert(sut(max_safe) < max<smoothing_factor_t>());
static_assert(sut(max_safe + cutoff_step_t::literal(1)) == max<smoothing_factor_t>());
static_assert(sut(max<cutoff_step_t>()) == max<smoothing_factor_t>());

constexpr auto exhaustive_small_domain() noexcept -> bool
{
    using small_step_t = fixed_t<uint8_t, 4>;
    using small_alpha_t = fixed_t<uint8_t, 8>;

    auto const map = alpha_map_t<small_alpha_t>{};
    auto previous = uint16_t{};

    for (auto raw = uint16_t{}; raw <= max<uint8_t>(); ++raw)
    {
        auto const input = small_step_t::literal(static_cast<uint8_t>(raw));
        auto const actual = uint16_t{map(input).value};
        auto const expected = raw > 239 ? uint16_t{255} : static_cast<uint16_t>((raw << 8) / (raw + 16));

        if (actual != expected) return false;
        if (actual < previous) return false;
        if (raw != 0 && raw <= 239 && actual == 0) return false;
        previous = actual;
    }

    return true;
}
static_assert(exhaustive_small_domain());

} // namespace
} // namespace crv::pipeline::filters::one_euro
