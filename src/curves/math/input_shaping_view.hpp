// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point wrapper for kernel shaping evaluation.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/input_shaping.hpp>

namespace curves {

/*!
  InputShapingView

  Non-owning view of kernel shaping parameters with floating-point evaluation.

  Calls the actual kernel shaping code. Uses fixed-point conversion to ensure
  the UI shows exactly what the driver calculates.
*/
class InputShapingView {
 public:
  explicit InputShapingView(const curves_shaping_params& params) noexcept
      : params_{&params} {}

  InputShapingView() noexcept = default;

  /*
    Apply shaping to raw velocity v.

    Input:  v in counts/ms, in floating-point
    Output: u in counts/ms, in floating-point

    This calls the kernel's fixed-point code and converts back.
  */
  auto operator()(real_t v) const noexcept -> real_t {
    if (!valid()) return v;

    s64 v_fixed = real_to_fixed(v);
    s64 u_fixed = curves_shaping_apply(v_fixed, params_);
    return fixed_to_real(u_fixed);
  }

  auto valid() const noexcept -> bool { return params_ != nullptr; }

  /* Ease-in parameters */
  auto ease_in_u_floor() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_in.u_floor) : 0.0;
  }
  auto ease_in_transition_v_begin() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_in.transition.v_begin) : 0.0;
  }
  auto ease_in_transition_v_width() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_in.transition.v_width) : 0.0;
  }
  auto ease_in_u_lag() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_in.u_lag) : 0.0;
  }

  /* Ease-out parameters */
  auto ease_out_transition_v_begin() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_out.transition.v_begin) : 0.0;
  }
  auto ease_out_transition_v_width() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_out.transition.v_width) : 0.0;
  }
  auto ease_out_u_ceiling() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_out.u_ceiling) : 0.0;
  }

  /* Derived boundaries */
  auto ease_in_transition_v_end() const noexcept -> real_t {
    return ease_in_transition_v_begin() + ease_in_transition_v_width();
  }
  auto ease_out_transition_v_end() const noexcept -> real_t {
    return ease_out_transition_v_begin() + ease_out_transition_v_width();
  }

 private:
  const curves_shaping_params* params_;
};

}  // namespace curves
