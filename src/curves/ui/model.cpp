#include "model.hpp"
#include <algorithm>

namespace curves::lib {

auto Point2D::distance_sq(const Point2D& other) const -> double {
  auto dx = x - other.x;
  auto dy = y - other.y;
  return (dx * dx) + (dy * dy);
}

Model::Model() {
  // Initialize with a simple 3-point curve
  points_.push_back({0.0, 1.0});  // Start point (bottom-left)
  points_.push_back({0.5, 0.5});  // Mid point
  points_.push_back({1.0, 0.0});  // End point (top-right)
}

auto Model::get_value_at(double x) const -> double {
  if (points_.empty()) {
    return 0.0;
  }

  // Find the two points x is between
  for (std::size_t i = 0; i < points_.size() - 1; ++i) {
    const auto& p1 = points_[i];
    const auto& p2 = points_[i + 1];

    if (x >= p1.x && x <= p2.x) {
      double t = (x - p1.x) / (p2.x - p1.x);
      // Linear interpolation (lerp)
      return p1.y + t * (p2.y - p1.y);
    }
  }
  // Default to first or last point's Y
  return (x < points_[0].x) ? points_[0].y : points_.back().y;
}

auto Model::move_point(std::size_t index, Point2D new_pos) -> void {
  if (index >= points_.size()) {
    return;  // Or throw
  }

  // Clamp position to 0.0-1.0 range
  new_pos.x = std::clamp(new_pos.x, 0.0, 1.0);
  new_pos.y = std::clamp(new_pos.y, 0.0, 1.0);

  // Prevent points from crossing in X (simplified)
  if (index > 0 && new_pos.x < points_[index - 1].x) {
    new_pos.x = points_[index - 1].x;
  }
  if (index < points_.size() - 1 && new_pos.x > points_[index + 1].x) {
    new_pos.x = points_[index + 1].x;
  }

  points_[index] = new_pos;
}

auto Model::get_points() const -> const std::vector<Point2D>& {
  return points_;
}

auto Model::get_point_count() const -> std::size_t { return points_.size(); }

}  // namespace curves::lib
