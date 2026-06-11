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

    auto initialize(int argc, char* argv[]) -> bool;
    auto run() -> int;

private:
    std::unique_ptr<config_store_t> store_;
    model::root_t model_root_;

    // ordered
    std::unique_ptr<QGuiApplication> gui_app_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    QUndoStack qt_undo_stack_;
    std::unique_ptr<command::stack_t<command::qt::stack_adapter_t<QUndoStack>>> undo_stack_;
    std::unique_ptr<curve_property_model_t> property_model_;
};

} // namespace crv
