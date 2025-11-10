#pragma once

#include <cstddef>  // For std::size_t
#include <vector>

namespace curves::lib {

// A simple, pure-C++ data structure. No Qt types.
struct Point2D {
  double x{0.0};
  double y{0.0};

  // Helper for distance calculation
  auto distance_sq(const Point2D& other) const -> double;
};

// Represents the core acceleration curve data.
// This is our "Model". It knows nothing about UI widgets.
class Model {
 public:
  Model();

  // Gets the interpolated Y value for a given X.
  // (Simplified to linear interpolation for this example)
  auto get_value_at(double x) const -> double;

  // Moves a point to a new position.
  auto move_point(std::size_t index, Point2D new_pos) -> void;

  // --- Getters for rendering ---
  auto get_points() const -> const std::vector<Point2D>&;
  auto get_point_count() const -> std::size_t;

 private:
  // The "source of truth" for the curve data.
  std::vector<Point2D> points_;
};

}  // namespace curves::lib
