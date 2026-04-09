// SPDX-License-Identifier: MIT

/// \file
/// \brief linear algebra types for scalars, vectors, and matrices
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <array>
#include <type_traits>

namespace crv::math {

/// multidimensional linear algebra type
///
/// Generalizes scalars, vectors, and matrices. Supports addition and multiplication for all combinations of types, and
/// division by scalar.
template <typename element_t, std::size_t... dims> struct linear_t;

/// n-d linear = 1-d array of (n-1)-d linears
template <typename element_tp, std::size_t dim, std::size_t... dims>
struct linear_t<element_tp, dim, dims...> : std::array<linear_t<element_tp, dims...>, dim>
{
    using element_t = element_tp;

    constexpr auto operator<=>(linear_t const& src) const noexcept -> auto = default;
    constexpr auto operator==(linear_t const& src) const noexcept -> bool  = default;
};

/// 1-d array of elements
template <typename element_tp, std::size_t dim> struct linear_t<element_tp, dim> : std::array<element_tp, dim>
{
    using element_t = element_tp;

    constexpr auto operator<=>(linear_t const& src) const noexcept -> auto = default;
    constexpr auto operator==(linear_t const& src) const noexcept -> bool  = default;
};

// --------------------------------------------------------------------------------------------------------------------
// scalars
// --------------------------------------------------------------------------------------------------------------------

template <typename element_t> using scalar_t = element_t;

// --------------------------------------------------------------------------------------------------------------------
// vectors
// --------------------------------------------------------------------------------------------------------------------

template <typename element_t, std::size_t size> using vector_t = linear_t<element_t, size>;

using vec2i_t = vector_t<int_t, 2>;
using vec3i_t = vector_t<int_t, 3>;
using vec2f_t = vector_t<float_t, 2>;
using vec3f_t = vector_t<float_t, 3>;

// --------------------------------------------------------------------------------------------------------------------
// matrices
// --------------------------------------------------------------------------------------------------------------------

template <typename element_t, std::size_t rows, std::size_t cols> using matrix_t = linear_t<element_t, rows, cols>;

using mat2x2i_t = matrix_t<int_t, 2, 2>;
using mat2x3i_t = matrix_t<int_t, 2, 3>;
using mat3x3i_t = matrix_t<int_t, 3, 3>;
using mat3x4i_t = matrix_t<int_t, 3, 4>;
using mat4x4i_t = matrix_t<int_t, 4, 4>;
using mat2x2f_t = matrix_t<float_t, 2, 2>;
using mat2x3f_t = matrix_t<float_t, 2, 3>;
using mat3x3f_t = matrix_t<float_t, 3, 3>;
using mat3x4f_t = matrix_t<float_t, 3, 4>;
using mat4x4f_t = matrix_t<float_t, 4, 4>;

/// deduces promoted element types
template <typename left_t, typename right_t> using promoted_t = std::common_type_t<left_t, right_t>;

// --------------------------------------------------------------------------------------------------------------------
// implementation
// --------------------------------------------------------------------------------------------------------------------

/// unary negation
template <typename element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator-(linear_t<element_t, dim, dims...> const& linear) noexcept -> linear_t<element_t, dim, dims...>
{
    linear_t<element_t, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = -linear[index];

    return result;
}

/// linear + linear
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator+(linear_t<left_element_t, dim, dims...> const&  left,
                         linear_t<right_element_t, dim, dims...> const& right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left[index] + right[index];

    return result;
}

/// n-d linear + (n-1)-d linear
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator+(linear_t<left_element_t, dim, dims...> const& left,
                         linear_t<right_element_t, dims...> const&     right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left[index] + right;

    return result;
}

/// (n-1)-d linear + n-d linear
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator+(linear_t<left_element_t, dims...> const&       left,
                         linear_t<right_element_t, dim, dims...> const& right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left + right[index];

    return result;
}

/// linear + scalar
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator+(linear_t<left_element_t, dim, dims...> const& left,
                         scalar_t<right_element_t> const&              right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left[index] + right;

    return result;
}

/// scalar + linear
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator+(scalar_t<left_element_t> const&                left,
                         linear_t<right_element_t, dim, dims...> const& right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left + right[index];

    return result;
}

/// linear - linear
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator-(linear_t<left_element_t, dim, dims...> const&  left,
                         linear_t<right_element_t, dim, dims...> const& right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left[index] - right[index];

    return result;
}

/// n-d linear - (n-1)-d linear
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator-(linear_t<left_element_t, dim, dims...> const& left,
                         linear_t<right_element_t, dims...> const&     right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left[index] - right;

    return result;
}

/// (n-1)-d linear - n-d linear
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator-(linear_t<left_element_t, dims...> const&       left,
                         linear_t<right_element_t, dim, dims...> const& right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left - right[index];

    return result;
}

/// linear - scalar
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator-(linear_t<left_element_t, dim, dims...> const& left,
                         scalar_t<right_element_t> const&              right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left[index] - right;

    return result;
}

/// scalar - linear
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator-(scalar_t<left_element_t> const&                left,
                         linear_t<right_element_t, dim, dims...> const& right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left - right[index];

    return result;
}

/// matrix*matrix
///
/// This implementation is cache-friendly for small matrices. It reorders the naive ijk traversal order into ikj to
/// preserve a common factor and locality, and it zeros out row by row instead of all at once at the start.
///
/// Matrices much larger than 4x4 will require a tiling implementation.
template <typename left_element_t, typename right_element_t, std::size_t rows, std::size_t common_dim, std::size_t cols>
constexpr auto operator*(matrix_t<left_element_t, rows, common_dim> const&  left,
                         matrix_t<right_element_t, common_dim, cols> const& right) noexcept
    -> matrix_t<promoted_t<left_element_t, right_element_t>, rows, cols>
{
    // default initialize to preserve cache
    matrix_t<promoted_t<left_element_t, right_element_t>, rows, cols> result;

    // use ikj order to maximize cache
    for (auto row = 0u; row < rows; ++row)
    {
        // zero-initialize row
        for (auto col = 0u; col < cols; ++col) result[row][col] = 0;

        for (auto common = 0u; common < common_dim; ++common)
        {
            // save common factor
            auto const left_row_common = left[row][common];

            for (auto col = 0u; col < cols; ++col) result[row][col] += left_row_common * right[common][col];
        }
    }

    return result;
}

/// matrix*vector
template <typename left_element_t, typename right_element_t, std::size_t rows, std::size_t cols>
constexpr auto operator*(matrix_t<left_element_t, rows, cols> const& left,
                         vector_t<right_element_t, cols> const&      right) noexcept
    -> vector_t<promoted_t<left_element_t, right_element_t>, rows>
{
    vector_t<promoted_t<left_element_t, right_element_t>, rows> result;

    for (auto row = 0u; row < rows; ++row)
    {
        result[row] = 0;
        for (auto col = 0u; col < cols; ++col) result[row] += left[row][col] * right[col];
    }

    return result;
}

/// vector*matrix
template <typename left_element_t, typename right_element_t, std::size_t rows, std::size_t cols>
constexpr auto operator*(vector_t<left_element_t, rows> const&        left,
                         matrix_t<right_element_t, rows, cols> const& right) noexcept
    -> vector_t<promoted_t<left_element_t, right_element_t>, cols>
{
    vector_t<promoted_t<left_element_t, right_element_t>, cols> result{};

    for (auto row = 0u; row < rows; ++row)
    {
        for (auto col = 0u; col < cols; ++col) result[col] += left[row] * right[row][col];
    }

    return result;
}

/// vector*vector is elementwise
///
/// Normally, 1xn*nx1 -> 1x1 would be the inner product and nx1*1xm -> nxm would be the outer, but modern convention in
/// graphics is elementwise.
template <typename left_element_t, typename right_element_t, std::size_t size>
constexpr auto operator*(vector_t<left_element_t, size> const&  left,
                         vector_t<right_element_t, size> const& right) noexcept
    -> vector_t<promoted_t<left_element_t, right_element_t>, size>
{
    vector_t<promoted_t<left_element_t, right_element_t>, size> result;

    for (auto index = 0u; index < size; ++index) result[index] = left[index] * right[index];

    return result;
}

/// linear*scalar
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator*(linear_t<left_element_t, dim, dims...> const& left,
                         scalar_t<right_element_t> const&              right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left[index] * right;

    return result;
}

/// scalar*linear
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator*(scalar_t<left_element_t> const&                left,
                         linear_t<right_element_t, dim, dims...> const& right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left * right[index];

    return result;
}

/// linear/scalar
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator/(linear_t<left_element_t, dim, dims...> const& left,
                         scalar_t<right_element_t> const&              right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left[index] / right;

    return result;
}

/// scalar/linear = linear{scalar}/linear
template <typename left_element_t, typename right_element_t, std::size_t dim, std::size_t... dims>
constexpr auto operator/(scalar_t<left_element_t> const&                left,
                         linear_t<right_element_t, dim, dims...> const& right) noexcept
    -> linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...>
{
    linear_t<promoted_t<left_element_t, right_element_t>, dim, dims...> result;

    for (auto index = 0u; index < dim; ++index) result[index] = left / right[index];

    return result;
}

} // namespace crv::math
