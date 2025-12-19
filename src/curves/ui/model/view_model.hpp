// SPDX-License-Identifier: MIT
/*!
  \file
  \brief ViewModel for profile editor.

  ViewModel is the bridge between the config domain (Profile) and the UI
  layer. It provides:

  - Iteration methods for UI construction (for_each_curve_param).
  - A write method (set_value) that all edits go through.
  - Save functionality (applycurves_sp.

  The write method is currently trivial but serves as the hook point for future
  undo/redo support. All widgets must call set_value rather than modifying
  Params directly.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/config/profile.hpp>
#include <curves/config/profile_store.hpp>
#include <curves/math/spline.hpp>
#include <curves/ui/model/flat_visitor.hpp>
#include <memory>

namespace curves {

class ViewModel {
 public:
  explicit ViewModel(Profile profile) noexcept : profile_{std::move(profile)} {}

  /*!
    Sets a parameter's value.

    All writes must go through this method rather than calling param.value(x)
    directly. This allows us to add undo/redo recording in the future without
    changing widget code.

    Parameters to modify must be owned by this ViewModel's profile_ member.

    \param param The parameter to modify.
    \param new_value The new value to set.
  */
  template <typename T>
  auto set_value(Param<T>& param, T new_value) -> void {
    param.value(std::move(new_value));
  }

  /*!
    Iterates all parameters for specified curve type.

    Callback is invoked once for each Param in the curve's config,
    including the interpretation enum. Callback should accept `auto&& param`.

    \param curve Which curve's parameters to iterate.
    \param callback Callable invoked for each parameter.
  */
  template <typename Callback>
  auto for_each_curve_param(CurveType curve, Callback&& callback) -> void {
    profile_.curve_profile_entries.visit_config(curve, [&](auto& entry) {
      auto visitor = FlatVisitor{callback};
      entry.config.reflect(visitor);
      entry.interpretation.reflect(visitor);
    });
  }

  //! Returns currently selected curve type.
  [[nodiscard]] auto selected_curve() const noexcept -> CurveType {
    return profile_.curve_type.value();
  }

  //! Sets selected curve type.
  auto selected_curve(CurveType curve) -> void {
    set_value(profile_.curve_type, curve);
  }

  //! Provides access to curve_type param for UI binding.
  [[nodiscard]] auto curve_type_param() noexcept -> Param<CurveType>& {
    return profile_.curve_type;
  }

  //! Provides access to DPI param for UI binding.
  [[nodiscard]] auto dpi_param() noexcept -> Param<int_t>& {
    return profile_.dpi;
  }

  //! Provides access to sensitivity param for UI binding.
  [[nodiscard]] auto sensitivity_param() noexcept -> Param<double>& {
    return profile_.sensitivity;
  }

  /*!
    \brief Creates spline for currently selected curve.
  */
  [[nodiscard]] auto create_spline() const -> std::unique_ptr<curves_spline> {
    std::unique_ptr<curves_spline> result;
    profile_.curve_profile_entries.visit_config(
        selected_curve(), [&](const auto& curve_profile_entry) {
          // When we support gain curves, the choice is in
          // curve_profile_entry.interpretation.

          const auto curve = curve_profile_entry.config.create();
          result = std::make_unique<curves_spline>(
              curves::spline::create_spline(curves::TransferFunction{curve}));
        });
    return result;
  }

  //! Saves profile to given store.
  auto apply(ProfileStore& store) const -> void { store.save(profile_); }

 private:
  Profile profile_;
};

}  // namespace curves
