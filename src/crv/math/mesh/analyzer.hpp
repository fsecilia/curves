// SPDX-License-Identifier: MIT

/// \file
/// \brief error metric that samples at equioscillation extrema
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/mesh/measured_error.hpp>
#include <array>
#include <cmath>

namespace crv::equioscillation {

/// error metric approximating L-infinity norm at equioscillation extrema
///
/// This type evaluates the error between the generated payload and the ideal function at natural equioscillation
/// points, capturing the maximum error across the interval.
template <typename real_t, typename ideal_function_t, typename evaluator_t, typename quantizer_t,
          typename sample_locator_t>
struct error_metric_t
{
    ideal_function_t ideal_function;
    evaluator_t      evaluator;
    quantizer_t      quantizer;
    sample_locator_t sample_locator;

    auto operator()(auto const& payload, real_t left, real_t right) const noexcept -> measured_error_t<real_t>
    {
        auto const center = (right + left) * 0.5;
        auto const radius = (right - left) * 0.5;

        auto result = measured_error_t<real_t>{}; // magnitude is always 0

        auto const& sample_locations = sample_locator();
        static_assert(!std::empty(sample_locations), "at least 1 sample required");

        for (auto sample_location : sample_locations)
        {
            // sub exact center and boundries with no truncation error using cmov
            //
            // Applying this to left and right prevents truncation from landing the evaluated position outside of the
            // given range. We also apply it to center because it's trivial to and center tends to suffer from
            // truncation.
            auto ideal_position = center + radius * sample_location;
            if (sample_location == -1.0) ideal_position = left;
            if (sample_location == 0.0) ideal_position = center;
            if (sample_location == 1.0) ideal_position = right;

            auto const quantized_position = quantizer(ideal_position);

            // evaluate both ideal at ideal position and approxmation at quantized position, measure error as difference
            auto const ideal         = ideal_function(ideal_position);
            auto const approximation = evaluator(payload, quantized_position);
            auto const magnitude     = std::abs(ideal - approximation);

            // argmax on magnitude
            if (magnitude > result.magnitude)
            {
                result.position  = ideal_position;
                result.magnitude = magnitude;
            }
        }

        return result;
    }
};

} // namespace crv::equioscillation
