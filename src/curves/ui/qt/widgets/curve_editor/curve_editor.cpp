#include "curve_editor.hpp"
#include "ui_curve_editor.h"
#include <curves/spline.hpp>

CurveEditor::CurveEditor(QWidget* parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::CurveEditor>()) {
  m_ui->setupUi(this);

  m_visible_range = QRectF(0, 0, 100, 10);
  setMouseTracking(true);
}

CurveEditor::~CurveEditor() = default;

void CurveEditor::setSpline(
    std::shared_ptr<const curves_spline_table> spline_table) {
  m_spline_table = spline_table;
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

void CurveEditor::drawCurves(QPainter& painter) {
  auto pen_sensitivity = QPen{m_theme.curve_sensitivity};
  pen_sensitivity.setWidthF(1.0);

  auto pen_derivative = QPen{m_theme.curve_derivative};
  pen_derivative.setWidthF(1.1);

  const auto frac_bits = 32;
  const auto dx = (11.33 / width()) * m_visible_range.width();

  bool color = false;
  painter.setPen(pen_sensitivity);

  auto p = QPointF{std::max(m_visible_range.x(), 0.0), 0.0};
  auto x_fixed = curves::to_fixed(p.x(), frac_bits);
  if (x_fixed >= m_spline_table->max.x) return;

  auto y_fixed =
      curves_eval_spline_table(m_spline_table.get(), x_fixed, frac_bits);
  p.setY(curves::to_float<qreal>(y_fixed, frac_bits));

  while (p.x() < m_visible_range.right()) {
    auto p0 = p;

    p.setX(p.x() + dx);

    x_fixed = curves::to_fixed(p.x(), frac_bits);
    if (x_fixed >= m_spline_table->max.x) break;

    y_fixed =
        curves_eval_spline_table(m_spline_table.get(), x_fixed, frac_bits);
    p.setY(curves::to_float<qreal>(y_fixed, frac_bits));

    painter.drawLine(logicalToScreen(p0), logicalToScreen(p));

    if (color)
      painter.setPen(pen_sensitivity);
    else
      painter.setPen(pen_derivative);

    color = !color;
  }
}
