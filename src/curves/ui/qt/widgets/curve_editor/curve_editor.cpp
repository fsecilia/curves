#include "curve_editor.hpp"
#include "ui_curve_editor.h"
#include <curves/rendering/spline_evaluator.hpp>
#include <curves/rendering/spline_sampler.hpp>
#include <curves/spline.hpp>

CurveEditor::CurveEditor(QWidget* parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::CurveEditor>()) {
  m_ui->setupUi(this);

  m_visible_range = QRectF(0, 0, 100, 10);
  reallocateCurve();
  setMouseTracking(true);
}

CurveEditor::~CurveEditor() = default;

void CurveEditor::setSpline(
    std::shared_ptr<const curves_spline> spline) {
  m_spline = spline;
  update();
}

void CurveEditor::wheelEvent(QWheelEvent* event) {
  const auto factor = std::pow(1.001, event->angleDelta().y());

  const auto logical = screenToLogical(event->position());

  const auto new_width = m_visible_range.width() / factor;
  const auto new_height = m_visible_range.height() / factor;

  // Adjust Top/Left so the point under the mouse stays static
  const auto ratio_x =
      (logical.x() - m_visible_range.left()) / m_visible_range.width();
  const auto ratio_y =
      (logical.y() - m_visible_range.bottom()) / m_visible_range.height();

  const auto new_left = logical.x() - new_width * ratio_x;
  const auto new_bottom = logical.y() - new_height * ratio_y;
  const auto new_top = new_bottom - new_height;
  m_visible_range.setRect(new_left, new_top, new_width, new_height);

  queueReallocateCurve();

  update();
}

void CurveEditor::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_dragging = true;
    m_last_mouse_pos = event->pos();
  }
}

void CurveEditor::mouseMoveEvent(QMouseEvent* event) {
  if (m_dragging) {
    const auto delta = event->pos() - m_last_mouse_pos;

    const auto scale_x = m_visible_range.width() / width();
    const auto scale_y = m_visible_range.height() / height();

    // pan
    m_visible_range.translate(-delta.x() * scale_x, delta.y() * scale_y);

    m_last_mouse_pos = event->pos();

    update();
  }
}

void CurveEditor::mouseReleaseEvent(QMouseEvent*) { m_dragging = false; }

void CurveEditor::paintEvent(QPaintEvent*) {
  auto painter = QPainter{this};
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), m_theme.background);

  drawGrid(painter);
  drawCurves(painter);
}

void CurveEditor::resizeEvent(QResizeEvent*) { queueReallocateCurve(); }

void CurveEditor::queueReallocateCurve() { m_reallocate_curve_queued = true; }

void CurveEditor::tryReallocateCurve() {
  if (!m_reallocate_curve_queued) return;
  m_reallocate_curve_queued = false;

  reallocateCurve();
}

void CurveEditor::reallocateCurve() {
  const auto num_pixels = width();
  const auto num_samples = std::max(
      1, static_cast<int>(std::ceil(num_pixels / pixels_per_curve_step_x)));
  m_curve_step_x = static_cast<qreal>(m_visible_range.width()) / num_samples;
  m_curve_polygon.resizeForOverwrite(num_samples);
}

QPointF CurveEditor::screenToLogical(QPointF screen) {
  const auto x =
      m_visible_range.left() + (screen.x() / width()) * m_visible_range.width();
  const auto y = m_visible_range.bottom() -
                 (screen.y() / height()) * m_visible_range.height();
  return QPointF(x, y);
}

QPointF CurveEditor::logicalToScreen(QPointF logical) {
  const auto x = (logical.x() - m_visible_range.left()) /
                 m_visible_range.width() * width();
  const auto y = (m_visible_range.bottom() - logical.y()) /
                 m_visible_range.height() * height();
  return QPointF(x, y);
}

double CurveEditor::calcGridStep(double visible_range, int target_num_ticks) {
  const auto size = visible_range / target_num_ticks;
  const auto magnitude = std::pow(10.0, std::floor(std::log10(size)));
  const auto normalized = size / magnitude;

  double snapped_fraction;
  if (normalized < 1.5)
    snapped_fraction = 1.0;
  else if (normalized < 3.0)
    snapped_fraction = 2.0;
  else if (normalized < 7.0)
    snapped_fraction = 5.0;
  else
    snapped_fraction = 10.0;

  return snapped_fraction * magnitude;
}

void CurveEditor::drawGridX(QPainter& painter, QPen& pen_axis, QPen& pen,
                            double start, double step) {
  for (auto x = start; x <= m_visible_range.right() + step; x += step) {
    if (x < m_visible_range.left()) continue;

    const auto top = logicalToScreen(QPointF{x, m_visible_range.top()});
    const auto bottom = logicalToScreen(QPointF{x, m_visible_range.bottom()});

    // Draw line.
    painter.setPen(std::abs(x) < 1e-9 ? pen_axis : pen);
    painter.drawLine(top.x(), 0, bottom.x(), height());

    // Draw label.
    painter.setPen(m_theme.text);
    QString label = QString::number(x, 'g', 4);
    painter.drawText(bottom.x() + 5, height() - 5, label);
  }
}

void CurveEditor::drawGridY(QPainter& painter, QPen& pen_axis, QPen& pen,
                            double start, double step) {
  for (double y = start; y <= m_visible_range.bottom() + step; y += step) {
    if (y < std::min(m_visible_range.top(), m_visible_range.bottom())) continue;

    QPointF pLeft = logicalToScreen(QPointF{m_visible_range.left(), y});
    QPointF pRight = logicalToScreen(QPointF{m_visible_range.right(), y});

    // Draw line.
    painter.setPen(std::abs(y) < 1e-9 ? pen_axis : pen);
    painter.drawLine(0, pLeft.y(), width(), pRight.y());

    // Draw label.
    painter.setPen(m_theme.text);
    QString label = QString::number(y, 'g', 4);
    painter.drawText(5, pLeft.y() - 5, label);
  }
}

void CurveEditor::drawGrid(QPainter& painter) {
  // Set up pens.
  auto pen_axis = QPen{m_theme.grid_axis};
  pen_axis.setWidth(0);

  auto pen_major = QPen{m_theme.grid_major};
  pen_major.setWidth(0);

  auto pen_minor = QPen{m_theme.grid_minor};
  pen_minor.setStyle(Qt::DashLine);
  pen_minor.setWidth(0);

  // Draw vertical grid lines.
  const auto major_step_x = calcGridStep(m_visible_range.width(), 10);
  const auto major_start_x =
      std::floor(m_visible_range.left() / major_step_x) * major_step_x;
  drawGridX(painter, pen_axis, pen_major, major_start_x, major_step_x);

  // Draw horizontal grid lines.
  const auto major_step_y = calcGridStep(m_visible_range.height(), 10);
  const auto major_start_y =
      std::floor(std::min(m_visible_range.top(), m_visible_range.bottom()) /
                 major_step_y) *
      major_step_y;
  drawGridY(painter, pen_axis, pen_major, major_start_y, major_step_y);
}

void CurveEditor::drawPolyline(QPainter& painter, const QColor& color,
                               const QPolygonF& polyline) {
  auto pen = QPen{color};
  pen.setWidthF(2.13);
  painter.setPen(pen);
  painter.drawPolyline(polyline);
}

auto CurveEditor::drawCurves(QPainter& painter) -> void {
  const int total_pixels = width();
  if (total_pixels <= 0) return;

  const double x_start_view = m_visible_range.left();
  const double view_width = m_visible_range.width();
  const double dx_pixel = view_width / double(total_pixels);

  m_buffers.clear();
  m_buffers.reserve(total_pixels);

  curves::SplineSampler sampler(m_spline.get());

  for (int i = 0; i < total_pixels; ++i) {
    const double x_screen = i;
    const double x_logical = x_start_view + (x_screen * dx_pixel);
    if (x_logical < 0) continue;

    const auto sample = sampler.sample(x_logical);
    const auto values = curves::CurveEvaluator{}.compute(sample, x_logical);

    m_buffers.addSample(
        logicalToScreen(QPointF{x_logical, values.sensitivity}),
        logicalToScreen(QPointF{x_logical, values.sensitivity_deriv}),
        logicalToScreen(QPointF{x_logical, values.gain}),
        logicalToScreen(QPointF{x_logical, values.gain_deriv}));
  }

  drawPolyline(painter, m_theme.curve_sensitivity, m_buffers.sens);
  drawPolyline(painter, m_theme.curve_sensitivity_derivative,
               m_buffers.sens_deriv);
  drawPolyline(painter, m_theme.curve_gain, m_buffers.gain);
  drawPolyline(painter, m_theme.curve_gain_derivative, m_buffers.gain_deriv);

#if 0
  auto segment_pen = QPen{Qt::darkBlue};
  painter.setPen(segment_pen);
  for (auto knot = 0; knot < SPLINE_NUM_SEGMENTS; ++knot) {
    const auto knot_x =
        curves::Fixed::literal(curves::spline::locate_knot(knot)).to_real();
    painter.drawLine(logicalToScreen(QPointF(knot_x, -10000)),
                     logicalToScreen(QPointF(knot_x, 10000)));
  }
#endif
}
