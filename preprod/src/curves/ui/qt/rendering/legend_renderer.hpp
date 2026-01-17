// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Renders interactive legend in curve editor widget.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/ui/qt/rendering/trace.hpp>
#include <QRect>

class QPainter;
class QPoint;
class QSize;

namespace curves {

class LegendRenderer {
 public:
  auto updateLayout(const Traces& traces, const QFont& font,
                    const QSize& parentSize) noexcept -> void {
    const auto widestLabelWidth = findWidestLabelWidth(traces, font);

    // Anchor top right with margin.
    const auto width = kPadding * 3 + kSwatchWidth + widestLabelWidth;
    const auto height = kPadding * 2 + kItemCount * kItemHeight;
    m_rect = {parentSize.width() - width - kMargin, kMargin, width, height};
  }

  auto paint(QPainter& painter, const Traces& traces) const -> void {
    painter.setBrush(QColor{0, 0, 0, 127});
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(m_rect, 5, 5);

    auto f = painter.font();

    auto yLabel = m_rect.top() + kPadding;
    auto ySwatch = yLabel + kItemHeight / 2;
    for (size_t i = 0; i < kTraceType; ++i) {
      const auto& trace = traces.traces[i];
      const auto selected = traces.selected == static_cast<TraceType>(i);

      painter.setOpacity(trace.visible ? 1.0 : 0.5);

      // Swatch Line
      const auto lineWidth =
          selected ? TraceTheme::kThickLineWidth : TraceTheme::kThinLineWidth;
      painter.setPen(QPen{trace.theme.color, lineWidth});
      painter.drawLine(m_rect.left() + kPadding, ySwatch,
                       m_rect.left() + kPadding + kSwatchWidth, ySwatch);

      // Bold font if this is the currently selected curve for editing
      if (selected) {
        f.setBold(true);
        painter.setFont(f);
      }

      // Draw label.
      painter.setPen(Qt::white);
      const auto textRect =
          QRect{m_rect.left() + kPadding * 2 + kSwatchWidth, yLabel,
                m_rect.width() - kPadding, kItemHeight};
      painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, trace.label);

      // Restore font if we changed it
      if (selected) {
        f.setBold(false);
        painter.setFont(f);
      }

      yLabel += kItemHeight;
      ySwatch += kItemHeight;
    }
  }

  auto onMousePress(const QPoint& position, Traces& traces) const noexcept
      -> bool {
    if (!m_rect.contains(position)) {
      return false;
    }

    const auto relativeY = position.y() - m_rect.top() - kPadding;
    const auto itemIndex = relativeY / kItemHeight;

    if (itemIndex < 0 || itemIndex >= kItemCount) return false;

    traces.traces[itemIndex].visible = !traces.traces[itemIndex].visible;
    return true;
  }

 private:
  static constexpr int kPadding = 10;
  static constexpr int kMargin = 10;
  static constexpr int kSwatchWidth = 25;
  static constexpr int kItemHeight = 24;
  static constexpr int kItemCount = kTraceType;

  QRect m_rect{};

  auto findWidestLabelWidth(const Traces& traces, const QFont& font) noexcept
      -> int {
    auto boldFont = font;
    boldFont.setBold(true);
    const auto fontMetrics = QFontMetrics{boldFont};

    int result = 0;
    for (const auto& trace : traces.traces) {
      result = std::max(result, fontMetrics.horizontalAdvance(trace.label));
    }

    return result;
  }
};

}  // namespace curves
