// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/abs.hpp>
#include <complex>
#include <concepts>

namespace crv {

/// complex-step first derivative of any holomorphic scalar->scalar evaluator at a real point
///
/// step is 1e-200: small enough that the O(h^2) truncation term vanishes in double, and immune to cancellation because
/// the derivative is recovered from the imaginary part, never a difference of nearly-equal reals.
template <typename evaluator_t, std::floating_point real_t>
constexpr auto complex_step_derivative(evaluator_t const& eval, real_t x) -> double
{
    /// start with the taylor expansion of f(x + ih):
    /// f(x + ih) = f(x) + (ih)f'(x) + ((ih)^2/2!)f''(x) + ((ih)^3/3!)f'''(x) + ...
    /// This simplifies to:
    /// f(x + ih) = f(x) + (ih)f'(x) - (h^2/2!)f''(x) - i(h^3/3!)f'''(x) + ...
    /// Regroup real and imaginary terms:
    /// f(x + ih) = [f(x) - (h^2/2!)f''(x) + ...] + i[hf'(x) - (h^3/3!)f'''(x) + ...]
    /// Extract imaginary part:
    /// im(f(x + ih)) = hf'(x) - (h^3/3!)f'''(x) + ...
    /// Divide by sides by h:
    /// im(f(x + ih))/h = f'(x) - (h^2/3!)f'''(x) + ...
    /// Now this looks a lot like finite differences. Choose infintesimal h, h = 1e-200, and the h^n terms vanish:
    /// f'(x) ~= im(f(x + ih))/h
    /// No subtraction to catastrophically cancel. It just requires a holomorphic function.

    constexpr auto h = real_t{1e-200};
    auto const f = eval(std::complex{x, h});
    return f.imag() / h;
}

/// relative error with an absolute floor of 1, so the tolerance is sane whether the reference is large or near zero.
template <std::floating_point real_t> constexpr auto rel_error(real_t actual, real_t expected) -> double
{
    return abs(actual - expected) / max(abs(expected), 1.0);
}

} // namespace crv
