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
#include <climits>
#include <concepts>
#include <expected>
#include <numeric>

namespace crv::spline {
namespace {

enum class segment_error_reason_t
{
    bad_float,
    coefficient_overflow,
    intermediate_overflow
};

template <std::floating_point real_t> struct segment_error_t
{
    segment_error_reason_t reason;
    real_t left;
    real_t right;

    constexpr auto operator==(segment_error_t const&) const noexcept -> bool = default;
};

// --------------------------------------------------------------------------------------------------------------------
// layouts
// --------------------------------------------------------------------------------------------------------------------

struct field_layout_t
{
    int_t shift_width;
    uint64_t shift_mask;
    uint64_t valid_shift_mask;
    bool is_signed;
};

constexpr auto intermediate_layout = field_layout_t{
    .shift_width = 6,
    .shift_mask = 0x3F, // 6-bit unsigned [0, 63]
    .valid_shift_mask = 0x3F, // bits 6 and 7 must be 0
    .is_signed = false,
};

constexpr auto final_layout = field_layout_t{
    .shift_width = 7,
    .shift_mask = 0x7F, // 7-bit signed [-64, 63]
    .valid_shift_mask = 0x7F, // bit 7 must be 0
    .is_signed = true,
};

constexpr auto accumulator_width = 64;
constexpr auto min_final_shift = -64;
constexpr auto max_final_shift = 63;
constexpr auto field_width = 64;

// --------------------------------------------------------------------------------------------------------------------
// unpacking
// --------------------------------------------------------------------------------------------------------------------

constexpr auto log2_min_width = -16;

using mantissa_t = int64_t;
using shift_t = int8_t;
using shift_mask_t = uint8_t;

struct unpacked_field_t
{
    mantissa_t mantissa;
    shift_t shift;

    constexpr auto operator==(unpacked_field_t const&) const noexcept -> bool = default;
};
using packed_field_t = uint64_t; // [signed mantissa | unsigned shift]

constexpr auto fields_per_segment = 4;
using packed_segment_t = std::array<packed_field_t, fields_per_segment>;
using unpacked_segment_t = std::array<unpacked_field_t, fields_per_segment>;

struct field_unpacker_t
{
    constexpr auto operator()(packed_field_t packed_field, field_layout_t layout) const noexcept -> unpacked_field_t
    {
        auto const shift_masked = int_cast<shift_mask_t>(packed_field & layout.shift_mask);

        int8_t shift_val = shift_masked;
        if (layout.is_signed)
        {
            auto const bit_padding = sizeof(shift_t) * CHAR_BIT - layout.shift_width;
            shift_val = int_cast<shift_t>(static_cast<shift_t>(shift_masked << bit_padding) >> bit_padding);
        }

        return {
            .mantissa = static_cast<mantissa_t>(packed_field) >> layout.shift_width,
            .shift = shift_val,
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
        auto const layout = (field_index == fields_per_segment - 1) ? final_layout : intermediate_layout;
        return unpack_field(packed_segment[field_index], layout);
    }

    constexpr auto operator()(packed_segment_t const& packed_segment) const noexcept -> unpacked_segment_t
    {
        return unpacked_segment_t{
            unpack_field(packed_segment[0], intermediate_layout),
            unpack_field(packed_segment[1], intermediate_layout),
            unpack_field(packed_segment[2], intermediate_layout),
            unpack_field(packed_segment[3], final_layout),
        };
    }
};

struct segment_validator_t
{
    constexpr auto operator()(packed_segment_t const& packed_segment) const noexcept -> bool
    {
        for (auto field = 0; field < fields_per_segment - 1; ++field)
        {
            if (!is_field_valid(packed_segment[field], intermediate_layout)) return false;
        }
        return is_field_valid(packed_segment[fields_per_segment - 1], final_layout);
    }

private:
    constexpr auto is_field_valid(packed_field_t packed, field_layout_t layout) const noexcept -> bool
    {
        // extract the lowest byte containing the shift
        static_assert(sizeof(shift_t) == 1);
        auto const raw_shift_bits = packed & 0xFF;
        return (raw_shift_bits & ~layout.valid_shift_mask) == 0;
    }
};

// --------------------------------------------------------------------------------------------------------------------
// evaluation
// --------------------------------------------------------------------------------------------------------------------

template <is_fixed out_t> struct segment_evaluator_t
{
    constexpr auto operator()(unpacked_segment_t const& unpacked_segment, is_fixed auto const& dx) const noexcept
        -> out_t
    {
        using narrow_t = out_t::value_t;
        using wide_t = widened_t<narrow_t>;

        auto accumulator = unpacked_segment[0].mantissa;
        for (auto field_index = 1; field_index < fields_per_segment - 1; ++field_index)
        {
            auto const wide_product = static_cast<wide_t>(accumulator) * dx.value;

            auto const shift = unpacked_segment[field_index - 1].shift;
            auto const rounding_bias = (1ULL << shift) >> 1;
            auto const rounded_product = int_cast<narrow_t>((wide_product + rounding_bias) >> shift);

            auto const mantissa = unpacked_segment[field_index].mantissa;
            accumulator = rounded_product + mantissa;
        }

        // unroll final loop iteration and do it wide with one shift and one round, aligning to the final output radix

        auto const wide_product = static_cast<wide_t>(accumulator) * dx.value;

        auto const intermediate_shift = unpacked_segment[2].shift;
        auto const final_shift = unpacked_segment[3].shift;

        auto const shifted_c3 = static_cast<wide_t>(unpacked_segment[3].mantissa) << intermediate_shift;
        auto wide_accumulator = wide_product + shifted_c3;
        auto const total_shift = intermediate_shift + final_shift;

        // final shift is signed
        if (total_shift >= 0)
        {
            auto const final_rounding_bias = (static_cast<wide_t>(1) << total_shift) >> 1;
            wide_accumulator = (wide_accumulator + final_rounding_bias) >> total_shift;
        }
        else
        {
            wide_accumulator <<= -total_shift;
        }

        accumulator = std::saturating_cast<narrow_t>(wide_accumulator);
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

    constexpr auto operator()(real_t val) const noexcept -> std::expected<extracted_real_t, segment_error_reason_t>
    {
        auto const bits = std::bit_cast<unsigned_t>(val);
        auto const raw_exponent = (bits >> frac_bits) & exponent_mask;

        // inf and nan are not supported
        auto const bad_float = raw_exponent == exponent_mask;
        if (bad_float) return std::unexpected(segment_error_reason_t::bad_float);

        // ftz
        if (raw_exponent == 0) return {};
        auto const exponent = int_cast<signed_t>(raw_exponent) - exponent_bias - frac_bits;

        auto const raw_magnitude = (bits & frac_mask) | implicit_bit;
        auto mantissa = static_cast<signed_t>(raw_magnitude);
        auto const is_negative = (bits >> (float_width - 1)) != 0;
        if (is_negative) mantissa = -mantissa;

        return extracted_real_t{.mantissa = mantissa, .exponent = exponent};
    }
};

template <typename t_field_unpacker_t> struct field_packer_t
{
    using field_unpacker_t = t_field_unpacker_t;

    [[no_unique_address]] field_unpacker_t unpack_field;

    constexpr auto operator()(unpacked_field_t unpacked_field, field_layout_t layout) const noexcept
        -> std::expected<packed_field_t, segment_error_reason_t>
    {
        auto const packed_field = static_cast<packed_field_t>(
            (unpacked_field.mantissa << layout.shift_width) | (unpacked_field.shift & layout.shift_mask));

        if (unpacked_field != unpack_field(packed_field, layout))
        {
            return std::unexpected(segment_error_reason_t::coefficient_overflow);
        }
        return packed_field;
    }
};

template <typename t_extracted_real_t, is_fixed t_in_t, is_fixed t_out_t> struct segment_builder_t
{
    using extracted_real_t = t_extracted_real_t;
    using in_t = t_in_t;
    using out_t = t_out_t;

    static constexpr auto in_frac_bits = t_in_t::frac_bits;
    static constexpr auto out_frac_bits = t_out_t::frac_bits;

    int_t delta;
    int_t acc_exp;
    mantissa_t prev_mantissa;

    constexpr auto push(extracted_real_t const& next) noexcept -> unpacked_field_t
    {
        // zero mantissa can't dominate the scale; treat it as matching the accumulator.
        auto const next_exp = (next.mantissa == 0) ? acc_exp : next.exponent;
        auto const exp_gap = next_exp - acc_exp;

        // delta absorbs dx bit-growth; exp_gap lifts the accumulator when the next coeff is larger.
        auto const acc_shift = delta + std::max<int_t>(0, exp_gap);
        auto const coeff_shift = std::max<int_t>(0, -exp_gap);

        // flush out-of-scale terms; shift >= 64 means the value is below the dominant term's ULP.
        auto const adjusted_mantissa = (coeff_shift >= accumulator_width) ? 0 : (next.mantissa >> coeff_shift);
        auto const final_acc_mantissa = (acc_shift >= accumulator_width) ? 0 : prev_mantissa;
        auto const final_acc_shift = (acc_shift >= accumulator_width) ? 0 : acc_shift;

        acc_exp = std::max(acc_exp, next_exp);
        prev_mantissa = adjusted_mantissa;

        return unpacked_field_t{
            .mantissa = final_acc_mantissa,
            .shift = int_cast<int8_t>(final_acc_shift),
        };
    }

    constexpr auto finish() const noexcept -> unpacked_field_t
    {
        static constexpr auto max_mantissa = std::numeric_limits<int64_t>::max();
        static constexpr auto min_mantissa = std::numeric_limits<int64_t>::min();

        // final term: no successor, so the shift aligns directly to the output radix
        auto const final_shift = -acc_exp - out_frac_bits;
        auto const clamped_shift = std::clamp<int_t>(final_shift, min_final_shift, max_final_shift);
        auto const delta_shift = final_shift - clamped_shift;

        auto compensated_mantissa = prev_mantissa;

        if (delta_shift > 0)
        {
            compensated_mantissa = (delta_shift >= accumulator_width) ? 0 : (prev_mantissa >> delta_shift);
        }
        else if (delta_shift < 0)
        {
            auto const left_shift = -delta_shift;
            if (prev_mantissa == 0) { compensated_mantissa = 0; }
            else if (left_shift >= accumulator_width)
            {
                compensated_mantissa = (prev_mantissa > 0) ? max_mantissa : min_mantissa;
            }
            else
            {
                auto const max_safe = max_mantissa >> left_shift;
                auto const min_safe = min_mantissa >> left_shift;

                if (prev_mantissa > max_safe) { compensated_mantissa = max_mantissa; }
                else if (prev_mantissa < min_safe) { compensated_mantissa = min_mantissa; }
                else
                {
                    compensated_mantissa = prev_mantissa << left_shift;
                }
            }
        }

        return unpacked_field_t{
            .mantissa = compensated_mantissa,
            .shift = int_cast<shift_t>(clamped_shift),
        };
    }
};

template <typename t_float_extractor_t, typename t_field_packer_t, typename t_segment_builder_t, int_t min_log2_width>
struct segment_packer_t
{
    using float_extractor_t = t_float_extractor_t;
    using field_packer_t = t_field_packer_t;
    using segment_builder_t = t_segment_builder_t;

    using in_t = segment_builder_t::in_t;
    using out_t = segment_builder_t::out_t;
    using extracted_real_t = float_extractor_t::extracted_real_t;
    using real_t = float_extractor_t::real_t;
    using polynomial_t = cubic_polynomial_t<real_t>;

    static constexpr auto in_frac_bits = in_t::frac_bits;
    static constexpr auto out_frac_bits = out_t::frac_bits;

    static_assert(in_frac_bits + min_log2_width > 0);

    [[no_unique_address]] field_packer_t pack_field;
    [[no_unique_address]] float_extractor_t extract_float;

    constexpr auto operator()(polynomial_t const& polynomial, int_t log2_width) const noexcept
        -> std::expected<packed_segment_t, segment_error_reason_t>
    {
        auto const initial = extract_float(polynomial[0]);
        if (!initial) return std::unexpected(initial.error());

        auto builder = segment_builder_t{
            .delta = in_frac_bits + log2_width,
            .acc_exp = initial->exponent,
            .prev_mantissa = initial->mantissa,
        };

        packed_segment_t packed_segment;

        for (auto field_index = 0; field_index < fields_per_segment - 1; ++field_index)
        {
            auto const next = extract_float(polynomial[field_index + 1]);
            if (!next) return std::unexpected(next.error());

            auto const unpacked = builder.push(*next);
            auto const pack_result = pack_field(unpacked, intermediate_layout);
            if (!pack_result) return std::unexpected(pack_result.error());
            packed_segment[field_index] = *pack_result;
        }

        auto const final_unpacked = builder.finish();
        auto const final_pack = pack_field(final_unpacked, final_layout);
        if (!final_pack) return std::unexpected(final_pack.error());
        packed_segment[fields_per_segment - 1] = *final_pack;

        return packed_segment;
    }
};

// --------------------------------------------------------------------------------------------------------------------
// testing
// --------------------------------------------------------------------------------------------------------------------

using real_t = float_t;

struct spline_dynamic_segment_test_t : Test
{
    using out_t = fixed_t<int64_t, 45>;
    using in_t = fixed_t<int64_t, 44>;
    using segment_unpacker_t = segment_unpacker_t<field_unpacker_t>;
    using segment_evaluator_t = segment_evaluator_t<out_t>;
    using float_extractor_t = float_extractor_t<float64_t>;
    using field_packer_t = field_packer_t<field_unpacker_t>;
    using segment_builder_t = segment_builder_t<float_extractor_t::extracted_real_t, in_t, out_t>;
    using segment_packer_t = segment_packer_t<float_extractor_t, field_packer_t, segment_builder_t, log2_min_width>;
    using cubic_polynomial_t = cubic_polynomial_t<real_t>;

    segment_packer_t segment_packer;
    segment_unpacker_t segment_unpacker;
    segment_evaluator_t segment_evaluator;

    auto test(cubic_polynomial_t const& polynomial, int_t log2_width, real_t input, real_t expected) -> void
    {
        // double check float value
        auto const t = input;
        auto const oracle = ((polynomial[0] * t + polynomial[1]) * t + polynomial[2]) * t + polynomial[3];
        EXPECT_NEAR(expected, oracle, 5e-13);

        auto const packed_segment = segment_packer(polynomial, log2_width);
        ASSERT_TRUE(packed_segment.has_value());
        auto const unpacked_segment = segment_unpacker(*packed_segment);

        // check actual result
        auto const width = std::ldexp(1.0, log2_width);
        auto const dx = to_fixed<in_t>(input * width);
        auto const actual_fixed = segment_evaluator(unpacked_segment, dx);
        auto const actual_float = from_fixed<real_t>(actual_fixed);
        EXPECT_NEAR(expected, actual_float, 1e-10);
    }
};

#if 1
TEST_F(spline_dynamic_segment_test_t, pathological_integral)
{
    auto const polynomial = cubic_polynomial_t{0.0, 0.0, 1000.0, 0.0};

    // kaboom
    test(polynomial, 8, 256, 256 * 1000);
};
#endif

// --------------------------------------------------------------------------------------------------------------------
// parameterized tests
// --------------------------------------------------------------------------------------------------------------------

struct vector_t
{
    real_t input;
    real_t expected;

    friend auto operator<<(std::ostream& out, vector_t const& src) -> std::ostream&
    {
        return out << "{.input = " << src.input << ", .expected = " << src.expected << "}";
    }
};

struct spline_dynamic_segment_param_test_t : spline_dynamic_segment_test_t, WithParamInterface<vector_t>
{
    real_t const input = GetParam().input;
    real_t const expected = GetParam().expected;

    // hermite: p0 = 0.1, m0 = 1, p1 = 0.5, m1 = 1.2
    static constexpr auto polynomial = cubic_polynomial_t{1.4, -2.0, 1.0, 0.1};

    auto test(int_t log2_width) -> void
    {
        spline_dynamic_segment_test_t::test(polynomial, log2_width, input, expected);
    }
};

TEST_P(spline_dynamic_segment_param_test_t, log2_width_m8)
{
    test(-8);
}

TEST_P(spline_dynamic_segment_param_test_t, log2_width_m1)
{
    test(-1);
}

TEST_P(spline_dynamic_segment_param_test_t, log2_width_0)
{
    test(0);
}

TEST_P(spline_dynamic_segment_param_test_t, log2_width_1)
{
    test(1);
}

TEST_P(spline_dynamic_segment_param_test_t, log2_width_i)
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
INSTANTIATE_TEST_SUITE_P(vectors, spline_dynamic_segment_param_test_t, ValuesIn(vectors));

} // namespace
} // namespace crv::spline
