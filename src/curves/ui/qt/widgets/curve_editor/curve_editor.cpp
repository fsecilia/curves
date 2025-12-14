#include "curve_editor.hpp"
#include "ui_curve_editor.h"
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
  pen.setWidthF(1.1);
  painter.setPen(pen);
  painter.drawPolyline(polyline);
}

void CurveEditor::drawCurves(QPainter& painter) {
  const auto total_pixels = width();

  m_buffers.clear();
  m_buffers.reserve(total_pixels);

  const double dx_pixel = m_visible_range.width() / double(total_pixels);
  const double x_start_view = m_visible_range.left();

  int current_pixel_idx = 0;

  const double x_knot_1 =
      curves::Fixed::literal(curves::locate_knot(1)).to_real();

  for (int i = 0; i < SPLINE_NUM_SEGMENTS; ++i) {
    if (current_pixel_idx >= total_pixels) break;

    // --- A. Segment Bounds ---
    const double x_seg_start =
        curves::Fixed::literal(curves::locate_knot(i)).to_real();
    const double x_seg_end =
        curves::Fixed::literal(curves::locate_knot(i + 1)).to_real();
    const double seg_width = x_seg_end - x_seg_start;

    if (x_seg_end < x_start_view) continue;

    // Calculate which screen pixels cover this segment.
    // Project the segment's end into pixel space.
    const double end_pixel_float = (x_seg_end - x_start_view) / dx_pixel;
    const int end_pixel_idx =
        std::min(total_pixels, (int)std::ceil(end_pixel_float));

    // If this segment is visible, load its data.
    if (end_pixel_idx > current_pixel_idx) {
      // We do this once per segment, not per sample.
      const auto* coeffs = m_spline->segments[i].coeffs;
      const double a = curves::Fixed::literal(coeffs[0]).to_real();
      const double b = curves::Fixed::literal(coeffs[1]).to_real();
      const double c = curves::Fixed::literal(coeffs[2]).to_real();
      const double d = curves::Fixed::literal(coeffs[3]).to_real();

      // Pre-calc inverse width for normalizing t.
      const double inv_width = (seg_width > 0) ? (1.0 / seg_width) : 0.0;
      const double inv_width_sq = inv_width * inv_width;

      for (; current_pixel_idx < end_pixel_idx; ++current_pixel_idx) {
        // Determine logical X for this pixel center.
        const double x_screen = current_pixel_idx;
        const double x_logical = x_start_view + (x_screen * dx_pixel);

        // Calculate t (normalized 0..1 within segment).
        // x_logical = x_seg_start + t * seg_width
        const double t = (x_logical - x_seg_start) * inv_width;

        // Transfer Function T(x) = at^3 + bt^2 + ct + d
        const double transfer = ((a * t + b) * t + c) * t + d;

        // Gain G(x) = T'(x) (w.r.t logical x, not local)
        // dy/dx = (dy/dt) * (dt/dx) = P'(t) * (1/width)
        // P'(t) = 3at^2 + 2bt + c
        const double p_prime = (3.0 * a * t + 2.0 * b) * t + c;
        const double gain = p_prime * inv_width;

        // Gain Slope G'(x) = T''(x)
        // d2y/dx2 = P''(t) * (1/width)^2
        // P''(t) = 6at + 2b
        const double p_double_prime = 6.0 * a * t + 2.0 * b;
        const double gain_deriv = p_double_prime * inv_width_sq;

        double sens = 0.0;
        double sens_deriv = 0.0;

        if (i == 0) {
          // Scale segment 0 by x to cancel infinite 1/x noise near 0.
          // Ideal sensitivity near 0 is roughly the slope at 0: (c_term /
          // width) But strictly following your code: d_dt / dx_first_segment
          const double ideal_sens = p_prime / x_knot_1;

          // Avoid div/0
          const double raw_sens =
              (x_logical > 1e-9) ? (transfer / x_logical) : ideal_sens;

          const double blend = x_logical / x_knot_1;
          sens = ideal_sens + blend * (raw_sens - ideal_sens);

          // For derivative in the blend region, we can approximate or use the
          // raw formula. The raw formula usually holds up okay unless x is
          // extremely close to 0.
          if (x_logical < 1e-9)
            sens_deriv = 0;  // Flat at origin
          else
            sens_deriv = (gain - sens) / x_logical;

        } else {
          // Standard definition
          sens = transfer / x_logical;
          sens_deriv = (gain - sens) / x_logical;
        }

        // --- F. Store ---
        m_buffers.addSample(logicalToScreen({x_screen, sens}),
                            logicalToScreen({x_screen, sens_deriv}),
                            logicalToScreen({x_screen, gain}),
                            logicalToScreen({x_screen, gain_deriv}));
      }
    }
  }

  drawPolyline(painter, m_theme.curve_sensitivity, m_buffers.sens);
  drawPolyline(painter, m_theme.curve_sensitivity_derivative,
               m_buffers.sens_deriv);
  drawPolyline(painter, m_theme.curve_gain, m_buffers.gain);
  drawPolyline(painter, m_theme.curve_gain_derivative, m_buffers.gain_deriv);
}

#if 0
void CurveEditor::drawSpline(QPainter& painter, const curves_spline& spline) {
  auto sample = std::begin(m_curve_polygon);
  const auto samples_end = std::end(m_curve_polygon);

  auto p = QPointF{std::max(m_visible_range.x(), 0.0), 0.0};

  const auto dx_first_segment =
      curves::Fixed::literal(curves_spline_locate_knot(1)).to_real();

  if (p.x() < dx_first_segment) {
    const auto* coeffs = spline.segments[0].coeffs;

    const auto a = curves::Fixed::literal(coeffs[0]).to_real();
    const auto b = curves::Fixed::literal(coeffs[1]).to_real();
    const auto c = curves::Fixed::literal(coeffs[2]).to_real();
    const auto t = p.x();
    const auto d_dt = (a * t + b) * t + c;
    const auto sensitivity = d_dt / dx_first_segment;

    p.setY(sensitivity);
    *sample++ = logicalToScreen(p);
    p.setX(p.x() + m_curve_step_x);
  }

  while (sample != samples_end && p.x() < dx_first_segment) {
    // The first segment is basically dividing noise by 0, so it has an
    // envelope of 1/x. Here, we counter that by blending between an ideal
    // segment with 0 noise and the real, noisy segment. The blend cancels
    // out the envelope exactly.

    const auto* coeffs = spline.segments[0].coeffs;
    const auto a = curves::Fixed::literal(coeffs[0]).to_real();
    const auto b = curves::Fixed::literal(coeffs[1]).to_real();
    const auto c = curves::Fixed::literal(coeffs[2]).to_real();
    const auto t = p.x();
    const auto d_dt = (a * t + b) * t + c;
    const auto ideal_sensitivity = d_dt / dx_first_segment;

    const auto x_fixed = curves::Fixed{p.x()};
    const auto y_fixed =
        curves::Fixed::literal(curves::eval(&spline, x_fixed.value));
    const auto x_real = x_fixed.to_real();
    const auto y_real = y_fixed.to_real();
    const auto sensitivity = y_real / x_real;

    const auto blend = p.x() / dx_first_segment;

    p.setY(ideal_sensitivity + blend * (sensitivity - ideal_sensitivity));
    *sample++ = logicalToScreen(p);
    p.setX(p.x() + m_curve_step_x);
  }

  // The remaining segments are rendered directly.
  while (sample != samples_end) {
    const auto x_fixed = curves::Fixed{p.x()};
    const auto y_fixed =
        curves::Fixed::literal(curves::eval(&spline, x_fixed.value));

    const auto x_real = x_fixed.to_real();
    const auto y_real = y_fixed.to_real();
    const auto sensitivity = y_real / x_real;

    p.setY(sensitivity);
    *sample++ = logicalToScreen(p);
    p.setX(p.x() + m_curve_step_x);
  }

  painter.drawPolyline(m_curve_polygon);
}
#endif
