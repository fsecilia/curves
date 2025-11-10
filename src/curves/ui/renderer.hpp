#pragma once

#include <curves/ui/model.hpp>
#include <vector>

namespace curves::ui {

// Define abstract colors our presenter can use
enum class Color { Background, Primary, Primary_Light, White };

// This is the pure C++ abstract interface for rendering.
// It knows nothing about Qt, QPainter, or pixels.
// It operates entirely in normalized (0.0 - 1.0) coordinates,
// with (0,0) at the bottom-left.
class IRenderer {
 public:
  virtual ~IRenderer() = default;

  virtual auto set_pen(Color color, double width) -> void = 0;
  virtual auto set_brush(Color color) -> void = 0;
  virtual auto set_no_pen() -> void = 0;
  virtual auto set_no_brush() -> void = 0;

  virtual auto fill_background(Color color) -> void = 0;
  virtual auto draw_polyline(const std::vector<lib::Point2D>& points)
      -> void = 0;
  virtual auto draw_ellipse(lib::Point2D center, double rx, double ry)
      -> void = 0;
};

}  // namespace curves::ui
