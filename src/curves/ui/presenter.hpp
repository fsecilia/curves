#pragma once

#include <curves/ui/model.hpp>
#include <memory>
#include <optional>

// Forward-declare the abstract renderer interface
namespace curves::ui {
class IRenderer;
}

namespace curves::ui {

// This is our "presenter". It orchestrates the UI logic.
class Presenter {
 public:
  Presenter();

  // --- UI Actions (called by the View) ---

  // Handles a "click" at a given normalized position.
  // Returns true if a point was selected.
  auto on_mouse_press(lib::Point2D normalized_pos) -> bool;

  // Handles a "drag" to a normalized position.
  auto on_mouse_move(lib::Point2D normalized_pos) -> void;
  auto on_mouse_release() -> void;

  auto render(IRenderer& renderer) const -> void;

  // --- UI State (queried by the View) ---

  // Gets the points from the model for drawing.
  auto get_curve_points() const -> const std::vector<lib::Point2D>&;

  // Gets the index of the currently selected point, if any.
  auto get_selected_point_index() const -> std::optional<std::size_t>;

 private:
  // The presenter owns the Model
  std::unique_ptr<lib::Model> model_;

  // UI-specific state
  std::optional<std::size_t> selected_point_index_;

  // This click radius is already in normalized coordinates, so it's perfect.
  static constexpr double kClickRadius = 0.02;

  // Define rendering constants in normalized (0-1) coordinates
  // (Assuming a 640px window, 8px is ~0.0125)
  static constexpr double kPointRadius = 0.0125;
  static constexpr double kSelectedPointRadius = 0.015;
};

}  // namespace curves::ui
