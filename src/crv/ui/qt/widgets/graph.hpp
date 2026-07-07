// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/model/composed_curve.hpp>
#include <crv/ui/qt/packed_curve.hpp>
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

    Q_PROPERTY(QVariant curve READ curve WRITE setCurve NOTIFY curveChanged)

public:
    using jet_t = jet_t<float_t>;
    using curve_variant_t = qt::curve_variant_t;

    explicit GraphWidget(QQuickItem* parent = nullptr);

    auto geometryChange(QRectF const& new_geom, QRectF const& old_geom) -> void override;
    auto itemChange(ItemChange change, ItemChangeData const& data) -> void override;
    auto paint(QPainter* painter) -> void override;

    QRectF domain() const { return domain_; }
    void setDomain(QRectF const& domain);

    int dpi() const { return dpi_; }
    void setDpi(int dpi);

    auto curve() const -> QVariant { return qt::pack_curve(*curve_); }
    void setCurve(QVariant const& curve);

    Q_INVOKABLE auto pan(QPointF const& delta) -> void;
    Q_INVOKABLE auto zoom(QPointF const& angle_delta, QPointF const& mouse_pos) -> void;

signals:
    void domainChanged();
    void dpiChanged();
    void curveChanged();

private:
    auto sample_count() const -> int_t;
    auto drawCurves(QPainter* painter) -> void;
    auto updateCurves() -> void;
    auto on_curve_changed() -> void;

    std::optional<curve_variant_t> curve_;
    grid_renderer_t grid_renderer_;
    QRectF domain_{0.0, 0.0, 15.0, 15.0};
    int dpi_ = 0;

    static constexpr auto min_domain_width = float_t{1e-2};
    static constexpr auto min_domain_height = float_t{1e-2};

    mutable QPolygonF function_points_;
    mutable QPolygonF derivative_points_;
};

} // namespace crv
