// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "graph.hpp"
#include <crv/overloaded.hpp>
#include <crv/variant.hpp>
#include <QQuickWindow>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace crv {

GraphWidget::GraphWidget(QQuickItem* parent)
    : QQuickPaintedItem(parent),
      evaluator_{model::curves::synchronous_t::evaluator_t<float_t>{model::curves::synchronous_t::params_t<float_t>{
          .motivity = 21.420138304,
          .gamma = 1.25,
          .smooth = 0.5,
          .sync_speed = 0.75,
      }}}
{
    setAntialiasing(true);
    setSmooth(true);
}

void GraphWidget::setDomain(QRectF const& domain)
{
    auto clamped_domain = domain;
    if (clamped_domain.width() < min_domain_width) clamped_domain.setWidth(min_domain_width);
    if (clamped_domain.height() < min_domain_height) clamped_domain.setHeight(min_domain_height);

    // Prevent excessive negative panning (assuming 0 is physical floor)
    // if (clamped_domain.left() < -10.0) clamped_domain.moveLeft(-10.0);
    // if (clamped_domain.bottom() < -2.0) clamped_domain.moveBottom(-2.0);

    if (domain_ != clamped_domain)
    {
        domain_ = clamped_domain;
        emit domainChanged();
        on_curve_changed();
    }
}

void GraphWidget::setDpi(int dpi)
{
    if (dpi_ != dpi)
    {
        dpi_ = std::max(0, dpi);
        emit dpiChanged();
        on_curve_changed();
    }
}

void GraphWidget::setEvaluator(evaluator_variant_t evaluator)
{
    evaluator_ = std::move(evaluator);
    on_curve_changed();
}

auto GraphWidget::geometryChange(QRectF const& new_geom, QRectF const& old_geom) -> void
{
    QQuickPaintedItem::geometryChange(new_geom, old_geom);
    if (new_geom.size() != old_geom.size()) on_curve_changed();
}

auto GraphWidget::itemChange(ItemChange change, ItemChangeData const& data) -> void
{
    QQuickPaintedItem::itemChange(change, data);

    switch (change)
    {
        case ItemDevicePixelRatioHasChanged: on_curve_changed(); break;

        case ItemSceneChange:
            if (data.window) on_curve_changed();
            break;

        default: break;
    }
}

auto GraphWidget::paint(QPainter* painter) -> void
{
    painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    auto const rect = boundingRect();
    painter->fillRect(rect, QColor("#1e1e1e"));

    if (dpi_ == 0)
    {
        painter->setPen(QColor(255, 255, 255, 150));
        painter->drawText(rect, Qt::AlignCenter, "Enter mouse DPI to continue.");
        return;
    }

    grid_renderer_.draw(painter, rect, domain_);

    painter->save();

    auto const scale_x = rect.width() / domain_.width();
    auto const scale_y = rect.height() / domain_.height();

    painter->translate(0, rect.height());
    painter->scale(scale_x, -scale_y);
    painter->translate(-domain_.x(), -domain_.y());

    drawCurves(painter);

    painter->restore();
}

auto GraphWidget::sample_count() const -> int_t
{
    auto const* win = window();
    assert(win);
    return static_cast<int_t>(std::lround(width() * win->effectiveDevicePixelRatio()));
}

auto GraphWidget::drawCurves(QPainter* painter) -> void
{
    auto const pen_thickness = 3.1;

    auto function_pen = QPen{Qt::cyan, 0.0};
    function_pen.setWidthF(pen_thickness);
    function_pen.setCosmetic(true);
    painter->setPen(function_pen);
    painter->drawPolyline(function_points_);

    auto derivative_pen = QPen{Qt::magenta, 0.0};
    derivative_pen.setWidthF(pen_thickness);
    derivative_pen.setCosmetic(true);
    painter->setPen(derivative_pen);
    painter->drawPolyline(derivative_points_);
}

auto GraphWidget::updateCurves() -> void
{
    function_points_.clear();
    derivative_points_.clear();

    if (!window()) return;

    auto const samples = sample_count();
    // auto const min_x = domain_.left();
    auto const step = domain_.width() / samples;

    // auto const y_margin = domain_.height() * 0.1;
    // auto const y_lo = domain_.top() - y_margin;
    // auto const y_hi = domain_.bottom() + y_margin;

    std::visit(
        [&](auto const& curve) {
            auto const first = std::max<int_t>(static_cast<int_t>(std::floor(domain_.left() / step - 0.5)), 0);
            auto const last = static_cast<int_t>(std::ceil(domain_.right() / step - 0.5));

            for (int_t sample = first; sample < last; ++sample)
            {
                auto const x = step * (sample + 0.5);
                auto const y = curve(jet_t{x, 1.0});

                function_points_ << QPointF(x, primal(y));

                auto const derivative = tangent(y);
                using std::isfinite;
                // if (isfinite(tangent(y))) derivative_points_ << QPointF(x, std::clamp(tangent(y), y_lo, y_hi));
                if (isfinite(derivative)) derivative_points_ << QPointF(x, derivative);
            }
        },
        *evaluator_);
}

auto GraphWidget::on_curve_changed() -> void
{
    updateCurves();
    update();
}

} // namespace crv
