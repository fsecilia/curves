// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/config.hpp>
#include <crv/ui/command/stack.hpp>
#include <crv/ui/qt/command/stack_adapter.hpp>
#include <crv/ui/qt/i18n.hpp>
#include <crv/ui/qt/property_model.hpp>
#include <QApplication>
#include <QGuiApplication>
#include <QMessageBox>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTranslator>
#include <QUndoStack>
#include <memory>

namespace crv {

class config_store_t;

// main application
class app_t : public QApplication
{
    Q_OBJECT

public:
    explicit app_t(int& argc, char** argv);
    ~app_t() override;

    Q_PROPERTY(int activeCurveIndex READ activeCurveIndex NOTIFY activeCurveChanged)
    auto activeCurveIndex() const -> int { return static_cast<int>(model_root_.profile.active_curve.value()); }
    Q_INVOKABLE auto set_active_curve(int index) -> void;

    auto initialize() -> bool;

    auto notify(QObject* receiver, QEvent* event) -> bool override;

signals:
    void activeCurveChanged();

private:
    auto load_active_curve_model() -> void;

    static constexpr char const app_name[] = {"curves"};

    std::unique_ptr<config_store_t> store_;
    model::root_t model_root_;
    QStringList curve_names_;

    QTranslator translator_;
    i18n::qt::provider_t const provider_;

    std::unique_ptr<QQmlApplicationEngine> engine_;

    using stack_adapter_t = command::qt::stack_adapter_t;
    stack_adapter_t::stack_t command_stack_;
    stack_adapter_t command_stack_adapter_{command_stack_};

    std::unique_ptr<property_model_t> device_model_;
    std::unique_ptr<property_model_t> profile_model_;
    std::unique_ptr<property_model_t> scale_model_;
    std::unique_ptr<property_model_t> offset_model_;
    std::unique_ptr<property_model_t> floor_model_;
    std::unique_ptr<property_model_t> limit_model_;
    std::unique_ptr<property_model_t> specific_curve_model_;
};

} // namespace crv
