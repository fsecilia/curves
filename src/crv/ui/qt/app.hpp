// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/config.hpp>
#include <crv/ui/command/stack.hpp>
#include <crv/ui/qt/command/stack_adapter.hpp>
#include <crv/ui/qt/property_model.hpp>
#include <QGuiApplication>
#include <QMessageBox>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUndoStack>
#include <memory>

namespace crv {

class config_store_t;

// main application
class app_t : public QObject
{
    Q_OBJECT

public:
    explicit app_t(QObject* parent = nullptr);
    ~app_t() override;

    Q_PROPERTY(int activeCurveIndex READ activeCurveIndex NOTIFY activeCurveChanged)
    auto activeCurveIndex() const -> int { return static_cast<int>(model_root_.profile.active_curve.value()); }
    Q_INVOKABLE auto set_active_curve(int index) -> void;

    auto initialize(int argc, char* argv[]) -> bool;
    auto run() -> int;

signals:
    void activeCurveChanged();

private:
    auto load_active_curve_model() -> void;

    std::unique_ptr<config_store_t> store_;
    model::root_t model_root_;
    QStringList curve_names_;

    // ordered
    std::unique_ptr<QGuiApplication> gui_app_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    QUndoStack qt_undo_stack_;
    std::unique_ptr<command::stack_t<command::qt::stack_adapter_t<QUndoStack>>> undo_stack_;

    std::unique_ptr<property_model_t> device_model_;
    std::unique_ptr<property_model_t> profile_model_;
    std::unique_ptr<property_model_t> scale_model_;
    std::unique_ptr<property_model_t> offset_model_;
    std::unique_ptr<property_model_t> floor_model_;
    std::unique_ptr<property_model_t> limit_model_;
    std::unique_ptr<property_model_t> specific_curve_model_;
};

} // namespace crv
