#include "painter_renderer.hpp"
#include <QBrush>
#include <QPen>
#include <QPolygonF>

QtPainterRenderer::QtPainterRenderer(QPainter* painter, const QSizeF& item_size)
    : painter_(painter), item_size_(item_size) {
  Q_ASSERT(painter_);
}

auto QtPainterRenderer::set_pen(curves::ui::Color color, double width) -> void {
  painter_->setPen(QPen(to_qt_color(color), width));
}

auto QtPainterRenderer::set_brush(curves::ui::Color color) -> void {
  painter_->setBrush(QBrush(to_qt_color(color)));
}

auto QtPainterRenderer::set_no_pen() -> void { painter_->setPen(Qt::NoPen); }

auto QtPainterRenderer::set_no_brush() -> void {
  painter_->setBrush(Qt::NoBrush);
}

auto QtPainterRenderer::fill_background(curves::ui::Color color) -> void {
  painter_->fillRect(QRectF(QPointF(0, 0), item_size_), to_qt_color(color));
}

auto QtPainterRenderer::draw_polyline(
    const std::vector<curves::lib::Point2D>& points) -> void {
  QPolygonF polyline;
  for (const auto& vm_point : points) {
    polyline.append(denormalize_position(vm_point));
  }
  painter_->drawPolyline(polyline);
}

auto QtPainterRenderer::draw_ellipse(curves::lib::Point2D center, double rx,
                                     double ry) -> void {
  // Denormalize center
  auto qt_center = denormalize_position(center);

  // Denormalize radii (which are in normalized units)
  // We only use width for both to keep circles circular
  auto pixel_rx = rx * item_size_.width();
  auto pixel_ry = ry * item_size_.width();  // Use width to maintain aspect

  painter_->drawEllipse(qt_center, pixel_rx, pixel_ry);
}

auto QtPainterRenderer::denormalize_position(
    const curves::lib::Point2D& vm_pos) const -> QPointF {
  // Convert from our (0,0) bottom-left to Qt's (0,0) top-left
  // And denormalize from (0, 1) to (0, width/height)
  return {
      vm_pos.x * item_size_.width(),
      (1.0 - vm_pos.y) * item_size_.height()  // Invert Y
  };
}

auto QtPainterRenderer::to_qt_color(curves::ui::Color color) const -> QColor {
  switch (color) {
    case curves::ui::Color::Background:
      return QColor("#404040");
    case curves::ui::Color::Primary:
      return QColor("#00AEEF");
    case curves::ui::Color::Primary_Light:
      return QColor("#33CFFF");  // A lighter blue
    case curves::ui::Color::White:
      return QColor("#FFFFFF");
    default:
      return QColor("#FF00FF");  // Magenta for errors
  }
}
