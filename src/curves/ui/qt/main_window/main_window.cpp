// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/
#include "main_window.hpp"
#include "ui_main_window.h"
#include <curves/config/enum.hpp>
#include <curves/math/spline.hpp>
#include <curves/math/transfer_function.hpp>
#include <curves/ui/qt/widgets/curve_parameter/curve_parameter.hpp>
#include <utility>

MainWindow::MainWindow(std::shared_ptr<curves::ViewModel> view_model,
                       std::shared_ptr<curves::ProfileStore> store,
                       QWidget* parent)
    : QMainWindow(parent),
      m_ui(std::make_unique<Ui::MainWindow>()),
      m_view_model{std::move(view_model)},
      m_store{std::move(store)} {
  m_ui->setupUi(this);

  // Wire up the Apply button
  connect(m_ui->pushButton, &QPushButton::clicked, this,
          &MainWindow::onApplyClicked);

  // Wire up the curve selector
  connect(m_ui->selected_curve, &QListWidget::currentRowChanged, this,
          &MainWindow::onCurveSelectionChanged);

  // Populate the curve selector with available curve types
  populateCurveSelector();

  // Select the curve that's in the profile (triggers widget rebuild)
  auto selected = static_cast<int>(m_view_model->selected_curve());
  m_ui->selected_curve->setCurrentRow(selected);

  // Initial curve display
  updateCurveDisplay();
}

MainWindow::~MainWindow() = default;

void MainWindow::populateCurveSelector() {
  // For now, we manually iterate the known curve types.
  // When we add more curves, we add them here and to the enum.
  m_ui->selected_curve->clear();

  // Add each curve type to the list widget
  // The row index corresponds to the enum value
  m_ui->selected_curve->addItem(
      QString::fromUtf8(curves::to_string(curves::CurveType::kSynchronous)));

  // Future curves would be added here:
  // m_ui->selected_curve->addItem(
  //     QString::fromUtf8(curves::to_string(curves::CurveType::kLinear)));
}

void MainWindow::onCurveSelectionChanged(int index) {
  if (index < 0) return;

  auto curve = static_cast<curves::CurveType>(index);
  m_view_model->set_selected_curve(curve);
  rebuildParameterWidgets(curve);
  updateCurveDisplay();
}

void MainWindow::rebuildParameterWidgets(curves::CurveType curve) {
  clearParameterWidgets();

  // Use the ViewModel to iterate all parameters for this curve
  m_view_model->for_each_curve_param(curve, [this](auto& param) {
    // Check if this is a Param<double> - those get spinbox widgets
    using ParamType = std::decay_t<decltype(param)>;

    if constexpr (std::is_same_v<ParamType, curves::Param<double>>) {
      auto* widget = new CurveParameter(m_view_model, param);

      // Connect the widget's changed signal to our handler
      connect(widget, &CurveParameter::valueChanged, this,
              &MainWindow::onParameterChanged);

      // Add to the list widget
      auto* item = new QListWidgetItem(m_ui->curve_parameters);
      item->setSizeHint(widget->sizeHint());
      m_ui->curve_parameters->addItem(item);
      m_ui->curve_parameters->setItemWidget(item, widget);

      m_parameter_widgets.push_back(widget);
    }

    // For enum params (like Interpretation), we'd create a combo box.
    // Skipping for MVP - can add later.
  });
}

void MainWindow::clearParameterWidgets() {
  // The QListWidget owns the items and widgets, so clearing it deletes them
  m_ui->curve_parameters->clear();
  m_parameter_widgets.clear();
}

void MainWindow::onParameterChanged() {
  // A parameter value changed - update the curve display
  updateCurveDisplay();
}

void MainWindow::onApplyClicked() {
  // Save current profile to disk
  m_view_model->apply(*m_store);
}

void MainWindow::updateCurveDisplay() {
  m_ui->curve_editor->setSpline(m_view_model->create_spline());
}
