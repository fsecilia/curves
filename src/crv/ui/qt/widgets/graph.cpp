// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "graph.hpp"
#include <crv/overloaded.hpp>
#include <crv/variant.hpp>
#include <algorithm>
#include <cmath>
#include <utility>

namespace crv {

GraphWidget::GraphWidget(QQuickItem* parent)
    : QQuickPaintedItem(parent), evaluator_{model::curves::log_normal_t::evaluator_t<float_t>{
                                     crv::model::curves::to_params(model::curves::log_normal_t::config_t{})}}
{
    setAntialiasing(true);
    setSmooth(true);
    on_curve_changed();
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
}

void GraphWidget::paint(QPainter* painter)
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

void GraphWidget::drawCurves(QPainter* painter)
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

void GraphWidget::updateCurves()
{
    function_points_.clear();
    derivative_points_.clear();

    float_t min_x = domain_.left();
    float_t max_x = domain_.right();
    float_t step = domain_.width() / 100.0;

    std::visit(
        [&](auto const& curve) {
            auto x0 = min_x;
            auto y0 = curve(jet_t{x0, 1.0});

            function_points_ << QPointF(x0, primal(y0));
            derivative_points_ << QPointF(x0, tangent(y0));

            while (x0 < max_x)
            {
                float_t x1 = std::min(x0 + step, max_x);
                jet_t y1 = curve(jet_t{x1, 1.0});

                sampleInterval(x0, x1, y0, y1);

                function_points_ << QPointF(x1, primal(y1));
                derivative_points_ << QPointF(x1, tangent(y1));

                x0 = x1;
                y0 = y1;
            }
        },
        *evaluator_);
}

void GraphWidget::sampleInterval(float_t x0, float_t x1, jet_t y0, jet_t y1) const
{
    using std::abs;

    auto const tangent_tolerance = 0.5;
    auto const min_step = domain_.width() / 10000.0;

    if (abs(tangent(y1) - tangent(y0)) > tangent_tolerance && (x1 - x0) > min_step)
    {
        auto mid_x = (x0 + x1) / 2.0;

        std::visit(
            [&](auto const& curve) {
                jet_t mid_y = curve(jet_t{mid_x, 1.0});

                sampleInterval(x0, mid_x, y0, mid_y);

                function_points_ << QPointF(mid_x, primal(mid_y));
                derivative_points_ << QPointF(mid_x, tangent(mid_y));

                sampleInterval(mid_x, x1, mid_y, y1);
            },
            *evaluator_);
    }
}

auto GraphWidget::on_curve_changed() -> void
{
    updateCurves();
    update();
}

} // namespace crv
