#pragma once

#include <QColor>
#include <QPainter>
#include <QSizeF>
#include <curves/ui/renderer.hpp>

// This is the concrete implementation of the IRenderer interface.
// It "translates" the abstract, normalized drawing calls into
// specific QPainter commands that operate in pixel coordinates.
class QtPainterRenderer : public curves::ui::IRenderer {
 public:
  QtPainterRenderer(QPainter* painter, const QSizeF& item_size);
  ~QtPainterRenderer() override = default;

  // --- IRenderer Overrides ---
  auto set_pen(curves::ui::Color color, double width) -> void override;
  auto set_brush(curves::ui::Color color) -> void override;
  auto set_no_pen() -> void override;
  auto set_no_brush() -> void override;

  auto fill_background(curves::ui::Color color) -> void override;
  auto draw_polyline(const std::vector<curves::lib::Point2D>& points)
      -> void override;
  auto draw_ellipse(curves::lib::Point2D center, double rx, double ry)
      -> void override;

 private:
  // Converts a normalized (0-1) presenter position to a Qt (pixel) position
  auto denormalize_position(const curves::lib::Point2D& vm_pos) const
      -> QPointF;

  // Converts abstract Color to a concrete QColor
  auto to_qt_color(curves::ui::Color color) const -> QColor;

  QPainter* painter_;
  const QSizeF& item_size_;
};
