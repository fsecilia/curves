// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Defines renderable traces, the actual curves the ui shows.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <QColor>
#include <QPainter>
#include <QString>

namespace curves {

enum class TraceType {
  gain_f,
  gain_df,
  sensitivity_f,
  sensitivity_df,
  count_
};
static constexpr auto kTraceType = static_cast<int>(TraceType::count_);

struct TraceTheme {
  static constexpr auto kThinLineWidth = 1.1;
  static constexpr auto kThickLineWidth = 2.6;
  QColor color;
};

struct Trace {
  QString label;
  const TraceTheme& theme;

  bool visible = true;
  QPolygonF samples = {};

  explicit Trace(QString label, const TraceTheme& theme) noexcept
      : label{std::move(label)}, theme{theme} {}

  auto clear() noexcept -> void {
    if (!visible) return;
    samples.clear();
  }
  auto reserve(int size) -> void {
    if (!visible) return;
    samples.reserve(size);
  }
  auto append(QPointF sample) -> void {
    if (!visible) return;
    samples.append(sample);
  }

  auto draw(QPainter& painter, bool selected) -> void {
    if (!visible) return;

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
  Trace traces[kTraceType];
  TraceType selected = TraceType::sensitivity_f;

  auto clear() noexcept -> void {
    for (auto& trace : traces) trace.clear();
  }

  auto reserve(int size) -> void {
    for (auto& trace : traces) trace.reserve(size);
  }

  auto append(const std::array<QPointF, kTraceType>& samples) -> void {
    for (auto trace_type = 0U; trace_type < kTraceType; ++trace_type) {
      traces[trace_type].append(samples[trace_type]);
    }
  }

  auto draw(QPainter& painter) -> void {
    for (auto trace_type = 0; trace_type < kTraceType; ++trace_type) {
      traces[trace_type].draw(painter,
                              trace_type == static_cast<int>(selected));
    }
  }
};

}  // namespace curves
