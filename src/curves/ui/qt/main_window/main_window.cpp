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
#include <cassert>
#include <utility>

// This should be determined programmatically by walking the set of curve
// configs.
static constexpr auto minVisibleParameters = 5;

MainWindow::MainWindow(std::shared_ptr<curves::ViewModel> view_model,
                       std::shared_ptr<curves::ProfileStore> store,
                       QWidget* parent)
    : QMainWindow(parent),
      m_ui(std::make_unique<Ui::MainWindow>()),
      m_view_model{std::move(view_model)},
      m_store{std::move(store)} {
  m_ui->setupUi(this);

  // Wire up Apply button
  connect(m_ui->pushButton, &QPushButton::clicked, this,
          &MainWindow::onApplyClicked);

  // Wire up curve selector.
  connect(m_ui->curveSelector, &QListWidget::currentRowChanged, this,
          &MainWindow::onCurveSelectionChanged);

  // Apply programtic css to curve selector.
  applyTabViewCss(*m_ui->curveSelector);

  // Populate curve selector with available curve types.
  populateCurveSelector();

  // Select the curve that's in the profile. This triggers rebuilding
  // curve_parameters.
  auto selected = static_cast<int>(m_view_model->selected_curve());
  m_ui->curveSelector->setCurrentRow(selected);

  // Programmatically determine min height to not show scroll bars on
  // curve_parameters.
  setListMinHeight(*m_ui->curve_parameters, minVisibleParameters);

  // Render first curve.
  updateCurveDisplay();
}

MainWindow::~MainWindow() = default;

void MainWindow::populateCurveSelector() {
  // For now, we manually iterate the known curve types.
  // When we add more curves, we add them here and to the enum.
  m_ui->curveSelector->clear();

  // Add each curve type to the list widget
  // The row index corresponds to the enum value
  m_ui->curveSelector->addItem(
      QString::fromUtf8(curves::to_string(curves::CurveType::kSynchronous)));

  // Future curves would be added here:
  // m_ui->curveSelector->addItem(
  //     QString::fromUtf8(curves::to_string(curves::CurveType::kLinear)));
}

void MainWindow::onCurveSelectionChanged(int index) {
  if (index < 0) return;

  auto curve = static_cast<curves::CurveType>(index);
  m_view_model->set_curveSelector(curve);
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

void MainWindow::applyTabViewCss(QWidget& widget) {
  const auto standardPalette = QApplication::palette();
  const auto window = standardPalette.color(QPalette::Window);
  const auto windowText = standardPalette.color(QPalette::WindowText);
  const auto highlight = standardPalette.color(QPalette::Highlight);
  const auto highlightText = standardPalette.color(QPalette::HighlightedText);

  const auto darkBackground = window.darker(200);
  const auto hoverBackground = darkBackground.lighter(150);

  const auto replacedTabViewCss = QString{tabViewCss.data()}.arg(
      darkBackground.name(), windowText.name(), hoverBackground.name(),
      highlight.name(), highlightText.name());

  widget.setStyleSheet(replacedTabViewCss);
}

void MainWindow::setListMinHeight(QListWidget& list, int minVisibleItems) {
  // Get item height from size hint.
  const auto sizeHintRole = list.model()->index(0, 0).data(Qt::SizeHintRole);
  const auto itemHeight = sizeHintRole.toSize().height();

  // Get vertical spacing between items.
  int spacing = list.spacing();

  // Calc total content height needed.
  int contentHeight =
      itemHeight * minVisibleItems + (spacing * std::max(0, minVisibleItems));

  // Add the list's own borders/frame.
  // frameWidth covers top and bottom, so we multiply by 2
  int borderHeight = list.frameWidth() * 2;

  // Apply.
  list.setMinimumHeight(contentHeight + borderHeight);
}

const std::string_view MainWindow::tabViewCss = R"(
    QListWidget {
      border: none;
      outline: 0px;
      background-color: %1;
    }

    QListWidget::item {
      padding: 10px 12px;
      margin: 4px 4px;
      border-radius: 5px;
      border: 1px solid transparent;
      color: %2;
    }

    QListWidget::item:hover {
      background-color: %3;
    }

    QListWidget::item:selected {
      background-color: %4;
      color: %5;
    }

    QListWidget::item:selected:!active {
      background-color: %4;
      color: %5;
    }
)";
