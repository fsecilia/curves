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
#include <string_view>

namespace curves {

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

  std::string_view label;
  QColor color;
};

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

}  // namespace curves
