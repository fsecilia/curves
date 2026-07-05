// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "grid_renderer.hpp"
#include <QString>
#include <cmath>

namespace crv {

auto grid_renderer_t::draw(QPainter* painter, QRectF const& screen_rect, QRectF const& domain) -> void
{
    painter->save();

    auto const scale_x = screen_rect.width() / domain.width();
    auto const scale_y = screen_rect.height() / domain.height();

    auto text_pen = QPen{QColor(255, 255, 255, 150)};
    text_pen.setCosmetic(true);

    auto grid_pen = QPen{QColor(255, 255, 255, 40)};
    grid_pen.setCosmetic(true);
    painter->setPen(grid_pen);

    auto const step_x = step(domain.width(), 10);
    auto const step_y = step(domain.height(), 8);

    // vertical lines, x-axis ticks
    auto const first_x = std::ceil(domain.left() / step_x) * step_x;
    for (auto x = first_x; x <= domain.right(); x += step_x)
    {
        auto const screen_x = (x - domain.left()) * scale_x;

        painter->setPen(grid_pen);
        painter->drawLine(QPointF{screen_x, 0}, QPointF{screen_x, screen_rect.height()});

        painter->setPen(text_pen);
        painter->drawText(QRectF{screen_x + 4, screen_rect.height() - 20, 50, 20}, Qt::AlignLeft | Qt::AlignBottom,
            QString::number(x, 'f', 1));
    }

    // horizontal lines, y-axis ticks
    auto const first_y = std::ceil(domain.top() / step_y) * step_y;
    for (auto y = first_y; y <= domain.bottom(); y += step_y)
    {
        auto const screen_y = screen_rect.height() - ((y - domain.top()) * scale_y);

        painter->setPen(text_pen);
        painter->drawText(
            QRectF{4, screen_y - 20, 50, 20}, Qt::AlignLeft | Qt::AlignBottom, QString::number(y, 'f', 1));

        painter->setPen(grid_pen);
        painter->drawLine(QPointF{0, screen_y}, QPointF{screen_rect.width(), screen_y});
    }

    painter->restore();
}

auto grid_renderer_t::step(float_t range, int_t target_tick_count) const noexcept -> float_t
{
    auto const raw = range / static_cast<float_t>(target_tick_count);
    auto const magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    auto const normalized = raw / magnitude;

    if (normalized > 5.0) return 10.0 * magnitude;
    if (normalized > 2.0) return 5.0 * magnitude;
    return 2.0 * magnitude;
}

} // namespace crv
