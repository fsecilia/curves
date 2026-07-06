// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/curves/evaluator.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/ui/qt/widgets/graph/grid_renderer.hpp>
#include <QPainter>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>
#include <QtQmlIntegration>

namespace crv {

class GraphWidget : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QRectF domain READ domain WRITE setDomain NOTIFY domainChanged)
    Q_PROPERTY(int dpi READ dpi WRITE setDpi NOTIFY dpiChanged)

public:
    using jet_t = jet_t<float_t>;
    using evaluator_variant_t = model::curves::evaluator_variant_t<float_t>;

    explicit GraphWidget(QQuickItem* parent = nullptr);

    auto paint(QPainter* painter) -> void override;

    QRectF domain() const { return domain_; }
    void setDomain(QRectF const& domain);

    int dpi() const { return dpi_; }
    void setDpi(int dpi);

    Q_INVOKABLE void setEvaluator(evaluator_variant_t evaluator);

signals:
    void domainChanged();
    void dpiChanged();

private:
    auto drawCurves(QPainter* painter) -> void;
    auto updateCurves() -> void;
    auto sampleInterval(float_t x0, float_t x1, jet_t y0, jet_t y1) const -> void;
    auto on_curve_changed() -> void;

    std::optional<evaluator_variant_t> evaluator_;
    grid_renderer_t grid_renderer_;
    QRectF domain_{0.0, 0.0, 15.0, 15.0};
    int dpi_ = 0;

    static constexpr auto min_domain_width = float_t{1.0};
    static constexpr auto min_domain_height = float_t{0.5};

    mutable QPolygonF function_points_;
    mutable QPolygonF derivative_points_;
};

} // namespace crv
