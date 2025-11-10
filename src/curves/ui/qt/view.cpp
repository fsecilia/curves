#include "view.hpp"
#include "painter_renderer.hpp"
#include <QMouseEvent>
#include <QPainter>

QtView::QtView(QQuickItem* parent)
    : QQuickPaintedItem(parent),
      presenter_(std::make_unique<curves::ui::Presenter>()) {
  // Enable mouse input
  setAcceptedMouseButtons(Qt::LeftButton);
  // Cache the initial size
  item_size_ = QSizeF(width(), height());
}

auto QtView::paint(QPainter* painter) -> void {
  // 1. Set rendering hints on the painter
  painter->setRenderHint(QPainter::Antialiasing);

  // 2. Create the concrete renderer implementation
  QtPainterRenderer renderer(painter, item_size_);

  // 3. Tell the presenter to render itself using the renderer
  // The presenter holds all the "what to draw" logic,
  // and the renderer holds all the "how to draw" logic.
  presenter_->render(renderer);
}

// --- Event Handlers (Translation Layer) ---

auto QtView::mousePressEvent(QMouseEvent* event) -> void {
  // 1. Translate Qt event to presenter data
  auto normalized_pos = normalize_position(event->position());

  // 2. Call presenter
  presenter_->on_mouse_press(normalized_pos);

  // 3. Trigger a repaint
  update();
  event->accept();
}

auto QtView::mouseMoveEvent(QMouseEvent* event) -> void {
  // 1. Translate
  auto normalized_pos = normalize_position(event->position());

  // 2. Call
  presenter_->on_mouse_move(normalized_pos);

  // 3. Repaint
  update();
  event->accept();
}

auto QtView::mouseReleaseEvent(QMouseEvent* event) -> void {
  presenter_->on_mouse_release();

  // 2. Repaint
  update();
  event->accept();
}

auto QtView::geometryChange(const QRectF& new_geometry,
                            const QRectF& old_geometry) -> void {
  // Cache the new size for coordinate conversion
  item_size_ = new_geometry.size();
  QQuickPaintedItem::geometryChange(new_geometry, old_geometry);
  update();  // Repaint on resize
}

// --- Coordinate Conversion Helpers ---

auto QtView::normalize_position(const QPointF& qt_pos) const
    -> curves::lib::Point2D {
  // Convert from Qt's (0,0) top-left to our (0,0) bottom-left
  // And normalize from (0, width) to (0, 1)
  return {
      qt_pos.x() / item_size_.width(),
      1.0 - (qt_pos.y() / item_size_.height())  // Invert Y
  };
}
