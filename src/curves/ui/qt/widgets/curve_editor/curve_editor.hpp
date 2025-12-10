#ifndef CURVE_EDITOR_HPP
#define CURVE_EDITOR_HPP

#include <QPainter>
#include <QWheelEvent>
#include <QWidget>
#include <memory>

struct curves_spline;

namespace Ui {
class CurveEditor;
}

struct Theme {
  QColor background = QColor(20, 20, 20);
  QColor grid_axis = QColor(180, 180, 180);
  QColor grid_major = QColor(60, 60, 60);
  QColor grid_minor = QColor(40, 40, 40);
  QColor text = QColor(200, 200, 200);
  QColor curve_sensitivity = Qt::magenta;
  QColor curve_derivative = Qt::yellow;
  QColor curve_gain = Qt::cyan;
};

class CurveEditor : public QWidget {
  Q_OBJECT

 public:
  explicit CurveEditor(QWidget* parent = nullptr);
  ~CurveEditor();

  void setSpline(std::shared_ptr<const curves_spline> spline);

 protected:
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  std::unique_ptr<Ui::CurveEditor> m_ui;

  Theme m_theme;

  constexpr static const qreal pixels_per_curve_step_x = 1.875;
  qreal m_curve_step_x;
  QPolygonF m_curve_polygon;
  std::shared_ptr<const curves_spline> m_spline;
  bool m_reallocate_curve_queued = false;
  void queueReallocateCurve();
  void tryReallocateCurve();
  void reallocateCurve();

  bool m_dragging = false;
  QPointF m_last_mouse_pos;

  QRectF m_visible_range;
  QPointF screenToLogical(QPointF screen);
  QPointF logicalToScreen(QPointF);

  double calcGridStep(double visible_range, int target_num_ticks);
  void drawGrid(QPainter& painter);
  void drawGridX(QPainter& painter, QPen& pen_axis, QPen& pen, double start,
                 double step);
  void drawGridY(QPainter& painter, QPen& pen_axis, QPen& pen, double start,
                 double step);

  void drawCurves(QPainter& painter);
};

#endif  // CURVE_EDITOR_HPP
