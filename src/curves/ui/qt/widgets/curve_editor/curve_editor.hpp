#ifndef CURVE_EDITOR_HPP
#define CURVE_EDITOR_HPP

#include <curves/config/curve.hpp>
#include <curves/ui/qt/rendering/legend_renderer.hpp>
#include <curves/ui/qt/rendering/trace.hpp>
#include <QPainter>
#include <QWheelEvent>
#include <QWidget>
#include <memory>

struct curves_spline;

namespace curves {
class SplineView;
class InputShapingView;
class CurveView;
}  // namespace curves

namespace Ui {
class CurveEditor;
}

struct Theme {
  QColor background = QColor(20, 20, 20);
  QColor grid_axis = QColor(150, 150, 150);
  QColor grid_major = QColor(60, 60, 60);
  QColor grid_minor = QColor(40, 40, 40);
  QColor grid_geometric_limit = QColor(0, 255, 0);
  QColor text = QColor(200, 200, 200);

  curves::TraceTheme traceThemes[curves::kTraceType] = {
      {Qt::magenta},
      {Qt::cyan},
      {Qt::red},
      {Qt::yellow},
  };
};

class CurveEditor : public QWidget {
  Q_OBJECT

 public:
  explicit CurveEditor(QWidget* parent = nullptr);
  ~CurveEditor() override;

  void setCurveView(curves::CurveView curveView);
  void setCurveInterpretation(curves::CurveInterpretation curveInterpretation);

  void enableDpiErrorState(bool enable);

 protected:
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void paintEvent(QPaintEvent*) override;

 private:
  std::unique_ptr<Ui::CurveEditor> m_ui;

  Theme m_theme;

  std::unique_ptr<curves::CurveView> m_curveView;
  curves::CurveInterpretation m_curveInterpretation{
      curves::CurveInterpretation::kGain};

  bool m_dpiErrorStateEnabled = false;

  bool m_dragging = false;
  QPointF m_last_mouse_pos;

  QRectF m_visible_range;
  QPointF screenToLogical(QPointF screen);
  QPointF logicalToScreen(QPointF logical);

  void drawDpiErrorState(QPainter& painter);

  double calcGridStep(double visible_range, int target_num_ticks);
  void drawGrid(QPainter& painter);
  void drawGridX(QPainter& painter, QPen& pen_axis, QPen& pen, qreal start,
                 qreal step);
  void drawGridY(QPainter& painter, QPen& pen_axis, QPen& pen, qreal start,
                 qreal step);

  curves::Traces m_traces{{
      curves::Trace{"g(v)", m_theme.traceThemes[0]},
      curves::Trace{"d/dv g(v)", m_theme.traceThemes[1]},
      curves::Trace{"s(v)", m_theme.traceThemes[2]},
      curves::Trace{"d/dv s(v)", m_theme.traceThemes[3]},
  }};
  curves::LegendRenderer m_legendRenderer;

  void drawTraces(QPainter& painter);
};

#endif  // CURVE_EDITOR_HPP
