// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/integer.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <bit>
#include <concepts>
#include <expected>

namespace crv::spline {
namespace {

enum class segment_error_reason_t
{
    dynamic_range_packing,
};

template <std::floating_point real_t> struct segment_error_t
{
    segment_error_reason_t reason;
    real_t left;
    real_t right;

    constexpr auto operator==(segment_error_t const&) const noexcept -> bool = default;
};

// --------------------------------------------------------------------------------------------------------------------
// unpacking
// --------------------------------------------------------------------------------------------------------------------

constexpr auto shift_width = 6;
constexpr auto shift_limit = 1ULL << shift_width;
constexpr auto shift_mask = shift_limit - 1;

struct unpacked_field_t
{
    int64_t mantissa;
    uint8_t shift;
};
using packed_field_t = uint64_t; // [signed mantissa | unsigned shift]
constexpr auto field_width = 64;

constexpr auto fields_per_segment = 4;
using packed_segment_t = std::array<packed_field_t, fields_per_segment>;
using unpacked_segment_t = std::array<unpacked_field_t, fields_per_segment>;

struct field_unpacker_t
{
    constexpr auto operator()(packed_field_t packed_field) const noexcept -> unpacked_field_t
    {
        return {
            .mantissa = static_cast<int64_t>(packed_field) >> shift_width,
            .shift = static_cast<uint8_t>(packed_field & shift_mask),
        };
    }
};

template <typename t_field_unpacker_t> struct segment_unpacker_t
{
    using field_unpacker_t = t_field_unpacker_t;

    [[no_unique_address]] field_unpacker_t unpack_field;

    constexpr auto operator()(packed_segment_t const& packed_segment, int_t field_index) const noexcept
        -> unpacked_field_t
    {
        return unpack_field(packed_segment[field_index]);
    }

    constexpr auto operator()(packed_segment_t const& packed_segment) const noexcept -> unpacked_segment_t
    {
        return unpacked_segment_t{
            unpack_field(packed_segment[0]),
            unpack_field(packed_segment[1]),
            unpack_field(packed_segment[2]),
            unpack_field(packed_segment[3]),
        };
    }
};

// --------------------------------------------------------------------------------------------------------------------
// evaluation
// --------------------------------------------------------------------------------------------------------------------

template <typename t_segment_unpacker_t, is_fixed out_t> struct segment_evaluator_t
{
    using segment_unpacker_t = t_segment_unpacker_t;

    [[no_unique_address]] segment_unpacker_t unpack_segment;

    // should we constrain fixed width and signedness?
    constexpr auto operator()(unpacked_segment_t const& unpacked_segment, is_fixed auto const& dx) const noexcept
        -> out_t
    {
        using narrow_t = out_t::value_t;
        using wide_t = widened_t<narrow_t>;

        auto accumulator = unpacked_segment[0].mantissa;
        for (auto field_index = 1; field_index < fields_per_segment; ++field_index)
        {
            auto const wide_product = static_cast<wide_t>(accumulator) * dx.value;

            auto const shift = unpacked_segment[field_index - 1].shift;
            auto const rounding_bias = (1ULL << shift) >> 1;
            auto const rounded_product = int_cast<narrow_t>((wide_product + rounding_bias) >> shift);

            auto const mantissa = unpacked_segment[field_index].mantissa;
            accumulator = rounded_product + mantissa;
        }

        // Apply the 4th shift to align D's scale to the final output scale
        auto const final_shift = unpacked_segment[fields_per_segment - 1].shift;
        auto const final_rounding_bias = (1ULL << final_shift) >> 1;
        accumulator = int_cast<narrow_t>((accumulator + final_rounding_bias) >> final_shift);

        return out_t::literal(accumulator);
    }
};

// --------------------------------------------------------------------------------------------------------------------
// packing
// --------------------------------------------------------------------------------------------------------------------

template <typename real_t> using cubic_polynomial_t = std::array<real_t, fields_per_segment>;

// extracts integer mantissa and exponent from a float
template <std::floating_point real_t> struct float_extractor_t;

template <> struct float_extractor_t<float64_t>
{
    using real_t = float64_t;
    using signed_t = int64_t;
    using unsigned_t = uint64_t;

    struct extracted_real_t
    {
        signed_t mantissa;
        int_t exponent;

        constexpr auto operator<=>(extracted_real_t const&) const noexcept -> auto = default;
        constexpr auto operator==(extracted_real_t const&) const noexcept -> bool = default;
    };

    // ieee constants for float64
    static constexpr auto float_width = 64;
    static constexpr auto frac_bits = signed_t{52};
    static constexpr auto frac_mask = unsigned_t{0x000FFFFFFFFFFFFF};
    static constexpr auto exponent_bias = signed_t{1023};
    static constexpr auto exponent_mask = unsigned_t{0x7FF};
    static constexpr auto implicit_bit = unsigned_t{0x0010000000000000};

    constexpr auto operator()(real_t val) const noexcept -> extracted_real_t
    {
        auto const bits = std::bit_cast<unsigned_t>(val);
        auto const raw_exponent = (bits >> frac_bits) & exponent_mask;

        assert(raw_exponent != exponent_mask && "float_extractor_t: inf and nan not supported");

        // ftz
        if (raw_exponent == 0) return {};
        auto const exponent = int_cast<signed_t>(raw_exponent) - exponent_bias - frac_bits;

        auto const raw_magnitude = (bits & frac_mask) | implicit_bit;
        auto mantissa = static_cast<signed_t>(raw_magnitude);
        auto const is_negative = (bits >> (float_width - 1)) != 0;
        if (is_negative) mantissa = -mantissa;

        return {mantissa, exponent};
    }
};

struct field_packer_t
{
    constexpr auto operator()(unpacked_field_t unpacked_field) const noexcept -> packed_field_t
    {
        assert(unpacked_field.mantissa == (unpacked_field.mantissa << shift_width) >> shift_width);
        assert(unpacked_field.shift == (unpacked_field.shift & shift_mask));
        return packed_field_t{static_cast<uint64_t>(unpacked_field.mantissa << shift_width) | unpacked_field.shift};
    }
};

template <typename t_float_extractor_t, typename t_field_packer_t, is_fixed t_in_t, is_fixed t_out_t>
struct segment_packer_t
{
    using float_extractor_t = t_float_extractor_t;
    using field_packer_t = t_field_packer_t;
    using in_t = t_in_t;
    using out_t = t_out_t;

    using extracted_real_t = float_extractor_t::extracted_real_t;
    using real_t = float_extractor_t::real_t;
    using polynomial_t = cubic_polynomial_t<real_t>;

    static constexpr auto in_frac_bits = in_t::frac_bits;
    static constexpr auto out_frac_bits = out_t::frac_bits;

    [[no_unique_address]] field_packer_t pack_field;
    [[no_unique_address]] float_extractor_t extract_float;

    constexpr auto operator()(polynomial_t const& polynomial, int_t log2_width) const noexcept
        -> std::expected<packed_segment_t, segment_error_reason_t>
    {
        packed_segment_t packed_segment;

        // pack all but final term, aligning relative shifts of each in turn
        extracted_real_t cur;
        extracted_real_t next = extract_float(polynomial[0]);
        for (auto field_index = 0; field_index < fields_per_segment - 1; ++field_index)
        {
            cur = next;
            next = extract_float(polynomial[field_index + 1]);

            auto const shift = in_frac_bits + log2_width + next.exponent - cur.exponent;

            // punt on large dynamic ranges; this is technically recoverable, but pathological enough to not matter
            auto const dynamic_range_too_large = shift < 0;
            if (dynamic_range_too_large) return std::unexpected{segment_error_reason_t::dynamic_range_packing};

            packed_segment[field_index] = pack_field(unpacked_field_t{
                .mantissa = cur.mantissa,
                .shift = int_cast<uint8_t>(shift),
            });
        }

        // align final term to output radix
        //
        // The final term has no successor term; its shift is responsible for aligning to the output format.
        auto final_shift = max(-next.exponent - out_frac_bits, int_t{0});
        packed_segment[fields_per_segment - 1]
            = pack_field(unpacked_field_t{.mantissa = next.mantissa, .shift = int_cast<uint8_t>(final_shift)});

        return packed_segment;
    }
};

using real_t = float_t;

struct vector_t
{
    real_t input;
    real_t expected;

    friend auto operator<<(std::ostream& out, vector_t const& src) -> std::ostream&
    {
        return out << "{.input = " << src.input << ", .expected = " << src.expected << "}";
    }
};

struct spline_dynamic_segment_test_t : TestWithParam<vector_t>
{
    using in_t = fixed_t<int64_t, 44>;
    using out_t = fixed_t<int64_t, 48>;
    using segment_unpacker_t = segment_unpacker_t<field_unpacker_t>;
    using segment_evaluator_t = segment_evaluator_t<segment_unpacker_t, out_t>;
    using float_extractor_t = float_extractor_t<float64_t>;
    using segment_packer_t = segment_packer_t<float_extractor_t, field_packer_t, in_t, out_t>;

    real_t const input = GetParam().input;
    real_t const expected = GetParam().expected;
    out_t const expected_fixed = to_fixed<out_t>(expected);

    segment_packer_t segment_packer;
    segment_unpacker_t segment_unpacker;
    segment_evaluator_t segment_evaluator;

    // hermite: p0 = 0.1, m0 = 1, p1 = 0.5, m1 = 1.2
    static constexpr auto polynomial = cubic_polynomial_t{1.4, -2.0, 1.0, 0.1};

    auto test(int_t log2_width) -> void
    {
        // double check float value
        auto const t = input;
        auto const oracle = ((polynomial[0] * t + polynomial[1]) * t + polynomial[2]) * t + polynomial[3];
        EXPECT_NEAR(expected, oracle, 1e-10);

        auto const packed_segment = segment_packer(polynomial, log2_width);
        EXPECT_TRUE(packed_segment.has_value());
        auto const unpacked_segment = segment_unpacker(*packed_segment);

        // check actual result
        auto const width = std::ldexp(1.0, log2_width);
        auto const dx = to_fixed<in_t>(input * width);
        auto const actual_fixed = segment_evaluator(unpacked_segment, dx);
        auto const actual_float = from_fixed<real_t>(actual_fixed);
        EXPECT_NEAR(expected, actual_float, 1e-12);
    }
};

TEST_P(spline_dynamic_segment_test_t, log2_width_m8)
{
    test(-8);
}

TEST_P(spline_dynamic_segment_test_t, log2_width_m1)
{
    test(-1);
}

TEST_P(spline_dynamic_segment_test_t, log2_width_0)
{
    test(0);
}

TEST_P(spline_dynamic_segment_test_t, log2_width_1)
{
    test(1);
}

TEST_P(spline_dynamic_segment_test_t, log2_width_i)
{
    test(8);
}

vector_t const vectors[] = {
    {0.0 / 1.0, 0.1},
    {1.0 / 4.0, 0.246875},
    {1.0 / 3.0, 0.262962962963},
    {1.0 / 2.0, 0.275},
    {2.0 / 3.0, 0.292592592593},
    {3.0 / 4.0, 0.315625},
    {1.0 / 1.0, 0.5},
};
INSTANTIATE_TEST_SUITE_P(vectors, spline_dynamic_segment_test_t, ValuesIn(vectors));

} // namespace
} // namespace crv::spline
