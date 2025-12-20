#ifndef CURVE_EDITOR_HPP
#define CURVE_EDITOR_HPP

#include <curves/config/curve.hpp>
#include <QPainter>
#include <QWheelEvent>
#include <QWidget>
#include <memory>

struct curves_spline;

namespace Ui {
class CurveEditor;
}

enum class TraceType {
  sensitivity_f,
  sensitivity_df_dx,
  gain_f,
  gain_df_dx,
  count_
};
static constexpr auto num_trace_types = static_cast<int>(TraceType::count_);

struct TraceTheme {
  static constexpr auto kThinLineWidth = 1.1;
  static constexpr auto kThickLineWidth = 2.6;

  QString label;
  QColor color;
};

struct Theme {
  QColor background = QColor(20, 20, 20);
  QColor grid_axis = QColor(180, 180, 180);
  QColor grid_major = QColor(60, 60, 60);
  QColor grid_minor = QColor(40, 40, 40);
  QColor text = QColor(200, 200, 200);

  TraceTheme traceThemes[num_trace_types] = {
      {"Sensitivity", Qt::red},
      {"Sensitivity Derivative", Qt::yellow},
      {"Gain", Qt::magenta},
      {"Gain Derivative", Qt::cyan}};
};

class CurveEditor : public QWidget {
  Q_OBJECT

 public:
  explicit CurveEditor(QWidget* parent = nullptr);
  ~CurveEditor();

  void setSpline(std::shared_ptr<const curves_spline> spline,
                 curves::CurveInterpretation curveInterpretation);

 protected:
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent*) override;

 private:
  std::unique_ptr<Ui::CurveEditor> m_ui;

  Theme m_theme;

  std::shared_ptr<const curves_spline> m_spline;
  curves::CurveInterpretation m_curveInterpretation{
      curves::CurveInterpretation::kGain};

  bool m_dragging = false;
  QPointF m_last_mouse_pos;

  QRectF m_visible_range;
  QPointF screenToLogical(QPointF screen);
  QPointF logicalToScreen(QPointF logical);

  double calcGridStep(double visible_range, int target_num_ticks);
  void drawGrid(QPainter& painter);
  void drawGridX(QPainter& painter, QPen& pen_axis, QPen& pen, double start,
                 double step);
  void drawGridY(QPainter& painter, QPen& pen_axis, QPen& pen, double start,
                 double step);

  struct Trace {
    const TraceTheme& theme;
    bool visible = true;
    QPolygonF samples = {};

    explicit Trace(const TraceTheme& theme) noexcept : theme{theme} {}

    auto clear() noexcept -> void { samples.clear(); }
    auto reserve(int size) -> void {
      samples.reserve(static_cast<std::size_t>(size));
    }
    auto append(QPointF sample) -> void { samples.append(sample); }

    auto draw(QPainter& painter, bool selected) -> void {
      auto color = QColor{theme.color};
      auto thickness = TraceTheme::kThinLineWidth;
      if (selected) {
        thickness = TraceTheme::kThickLineWidth;
      } else {
        color.setAlphaF(0.5);
      }

      auto pen = QPen{color};
      pen.setWidthF(thickness);
      painter.setPen(pen);

      painter.drawPolyline(samples);
    }
  };

  struct Traces {
    Trace traces[num_trace_types];
    TraceType selected = TraceType::sensitivity_f;

    auto clear() noexcept -> void {
      for (auto& trace : traces) trace.clear();
    }

    auto reserve(int size) -> void {
      for (auto& trace : traces) trace.reserve(size);
    }

    auto append(const std::array<QPointF, num_trace_types>& samples) -> void {
      for (auto trace_type = 0; trace_type < num_trace_types; ++trace_type) {
        traces[trace_type].append(samples[trace_type]);
      }
    }

    auto draw(QPainter& painter) -> void {
      for (auto trace_type = 0; trace_type < num_trace_types; ++trace_type) {
        traces[trace_type].draw(painter,
                                trace_type == static_cast<int>(selected));
      }
    }
  };

  Traces m_traces{{Trace{m_theme.traceThemes[0]}, Trace{m_theme.traceThemes[1]},
                   Trace{m_theme.traceThemes[2]},
                   Trace{m_theme.traceThemes[3]}}};

  void drawTraces(QPainter& painter);
};

#endif  // CURVE_EDITOR_HPP
