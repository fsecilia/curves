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

namespace crv::spline {

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

template <typename t_float_extractor_t, is_fixed t_in_t> struct field_packer_t
{
    using float_extractor_t = t_float_extractor_t;
    using in_t = t_in_t;

    using real_t = float_extractor_t::real_t;
    using extracted_real_t = float_extractor_t::extracted_real_t;

    static constexpr auto mantissa_bits = field_width - shift_width;
    static constexpr auto total_headroom = mantissa_bits - (float_extractor_t::frac_bits + 1) - 1;
    static constexpr auto in_frac_bits = in_t::frac_bits;

    // temporary sanity check during dev
    static_assert(total_headroom == 4);

    [[no_unique_address]] float_extractor_t extract_float;

    constexpr auto operator()(unpacked_field_t unpacked_field) const noexcept -> packed_field_t
    {
        assert(unpacked_field.mantissa == (unpacked_field.mantissa << shift_width) >> shift_width);
        assert(unpacked_field.shift == (unpacked_field.shift & shift_mask));
        return packed_field_t{static_cast<uint64_t>(unpacked_field.mantissa << shift_width) | unpacked_field.shift};
    }

    struct packing_step_result_t
    {
        packed_field_t packed_field;
        extracted_real_t modified_next;
    };

    constexpr auto operator()(extracted_real_t cur, extracted_real_t next, int_t log2_segment_width) const noexcept
        -> packing_step_result_t
    {
        auto shift = in_frac_bits + log2_segment_width + next.exponent - cur.exponent;

        // handle large dynamic range differences between terms
        if (shift < 0)
        {
            // relative left shift: cur is large and next is small
            //
            // The relative shift is negative. Scale cur up or scale next down to increase the exponent to zero.
            auto left_shift = -shift;

            // start by exhausting headroom
            //
            // When using float64, there are 4 empty bits in the container between the 53-bit mantissa and the 58-bit
            // field width. These can be consumed via shift without any loss of generality.
            auto headroom = std::min(left_shift, total_headroom);
            cur.mantissa <<= headroom;
            left_shift -= headroom;

            // reduce precision on next term
            //
            // if left shift still remains, scale the next term down by the remaining shift
            if (left_shift > 0)
            {
                if (left_shift >= field_width) next.mantissa = 0;
                else next.mantissa >>= left_shift;

                next.exponent += left_shift;
            }

            assert(left_shift == 0);
            shift = 0;
        }
        else if (shift >= field_width)
        {
            // relative right shift: cur is small and next is large
            //
            // If the right shift needed to align radices is larger than the field with, this term will be shifted
            // off entirely, but allowing that shift to actually run is UB. Instead, equivalently zero out both.
            cur.mantissa = 0;
            shift = 0;
        }

        return {
            .packed_field = operator()(unpacked_field_t{
                .mantissa = cur.mantissa,
                .shift = int_cast<uint8_t>(shift),
            }),
            .modified_next = next,
        };
    }
};

template <typename t_field_packer_t, is_fixed t_out_t> struct segment_packer_t
{
    using field_packer_t = t_field_packer_t;
    using out_t = t_out_t;

    using real_t = field_packer_t::real_t;

    static constexpr auto out_frac_bits = out_t::frac_bits;

    [[no_unique_address]] field_packer_t pack_field;

    // this exists at the wrong level
    field_packer_t::float_extractor_t const& extract_float = pack_field.extract_float;

    constexpr auto operator()(real_t a, real_t b, real_t c, real_t d, int_t log2_width) const noexcept
        -> packed_segment_t
    {
        packed_segment_t result;

        auto cur = extract_float(a);
        auto next = extract_float(b);
        auto packing_step = pack_field(cur, next, log2_width);
        result[0] = packing_step.packed_field;

        cur = packing_step.modified_next;
        next = extract_float(c);
        packing_step = pack_field(cur, next, log2_width);
        result[1] = packing_step.packed_field;

        cur = packing_step.modified_next;
        auto next_d = extract_float(d);

        // adjust d to hit the output frac_bits if it naturally falls short
        //
        // This absorbs any left shifts so the evaluator only shifts right.
        auto pre_shift = -next_d.exponent - out_frac_bits;
        if (pre_shift < 0)
        {
            auto const left_shift = -pre_shift;
            if (left_shift <= field_packer_t::total_headroom)
            {
                next_d.mantissa <<= left_shift;
                next_d.exponent -= left_shift;
            }
            else
            {
                next_d.mantissa = 0;
                next_d.exponent = -out_frac_bits;
            }
        }

        // align c*dx to d
        packing_step = pack_field(cur, next_d, log2_width);
        result[2] = packing_step.packed_field;

        // calc pure shift from d to the out_frac_bits
        auto final_d = packing_step.modified_next;
        auto final_shift = max(-final_d.exponent - out_frac_bits, int_t{0});

        result[3] = pack_field(unpacked_field_t{.mantissa = final_d.mantissa, .shift = int_cast<uint8_t>(final_shift)});

        return result;
    }
};

namespace {

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
    using field_packer_t = field_packer_t<float_extractor_t, in_t>;
    using segment_packer_t = segment_packer_t<field_packer_t, out_t>;

    real_t const input = GetParam().input;
    real_t const expected = GetParam().expected;
    out_t const expected_fixed = to_fixed<out_t>(expected);

    segment_packer_t segment_packer;
    segment_unpacker_t segment_unpacker;
    segment_evaluator_t segment_evaluator;

    // p0 = 0.1, m0 = 1, p1 = 0.5, m1 = 1.2
    static constexpr float64_t a = 1.4;
    static constexpr float64_t b = -2;
    static constexpr float64_t c = 1.0;
    static constexpr float64_t d = 0.1;
};

TEST_P(spline_dynamic_segment_test_t, log2_width_0)
{
    auto const log2_width = 0;
    auto const width = real_t{1 << log2_width};

    // double check float value
    auto const t = input / width;
    auto const oracle = ((a * t + b) * t + c) * t + d;
    EXPECT_NEAR(expected, oracle, 1e-10);

    auto const packed_segment = segment_packer(a, b, c, d, log2_width);
    auto const unpacked_segment = segment_unpacker(packed_segment);

    // check actual result
    auto const dx = to_fixed<in_t>(input);
    auto const actual_fixed = segment_evaluator(unpacked_segment, dx);
    auto const actual_float = from_fixed<real_t>(actual_fixed);
    EXPECT_NEAR(expected, actual_float, 1e-10);
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
INSTANTIATE_TEST_CASE_P(vectors, spline_dynamic_segment_test_t, ValuesIn(vectors));

} // namespace

} // namespace crv::spline
