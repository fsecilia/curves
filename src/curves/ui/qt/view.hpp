#pragma once

#include <QQuickPaintedItem>
#include <curves/ui/presenter.hpp>
#include <memory>

// Forward declarations for Qt types
class QPainter;
class QMouseEvent;

// This is the Qt "View" adapter.
// It subclasses QQuickPaintedItem to get a custom paint() method.
class QtView : public QQuickPaintedItem {
  Q_OBJECT
  Q_DISABLE_COPY(QtView)
  QML_ELEMENT

 public:
  explicit QtView(QQuickItem* parent = nullptr);
  ~QtView() override = default;

  // The main drawing function
  auto paint(QPainter* painter) -> void override;

  // Mouse event handlers
  auto mousePressEvent(QMouseEvent* event) -> void override;
  auto mouseMoveEvent(QMouseEvent* event) -> void override;
  auto mouseReleaseEvent(QMouseEvent* event) -> void override;

 protected:
  // Handle resizing
  auto geometryChange(const QRectF& new_geometry, const QRectF& old_geometry)
      -> void override;

 private:
  // --- Helper Functions ---

  // Converts a Qt (pixel) position to a normalized (0-1) presenter position
  auto normalize_position(const QPointF& qt_pos) const -> curves::lib::Point2D;

  // The View owns the presenter
  std::unique_ptr<curves::ui::Presenter> presenter_;

  // Cache the component's size
  QSizeF item_size_;
};
