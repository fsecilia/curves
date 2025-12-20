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

using namespace curves;

// This should be determined programmatically by walking the set of curve
// configs.
static constexpr auto minVisibleParameters = 4;

MainWindow::MainWindow(std::shared_ptr<ViewModel> view_model,
                       std::shared_ptr<ProfileStore> store, QWidget* parent)
    : QMainWindow(parent),
      m_ui(std::make_unique<Ui::MainWindow>()),
      m_view_model{std::move(view_model)},
      m_store{std::move(store)} {
  m_ui->setupUi(this);

  connectControls();

  populateCurveSelector();
  selectConfiguredCurve();
  constrainConfigHeight();

  // Render first curve.
  updateCurveDisplay();
}

MainWindow::~MainWindow() = default;

// ----------------------------------------------------------------------------

void MainWindow::onParameterChanged() {
  // A parameter value changed - update the curve display
  updateCurveDisplay();
}

void MainWindow::onCurveSelectionChanged(int index) {
  if (index < 0) return;

  auto curve = static_cast<CurveType>(index);
  m_view_model->selected_curve(curve);
  rebuildParameterWidgets(curve);
  updateCurveDisplay();
}

void MainWindow::onCurveInterpretation(bool checked,
                                       CurveInterpretation interpretation) {
  if (!checked) return;

  const auto prevInterpretation = m_curveInterpretationParam->value();
  if (interpretation == prevInterpretation) return;

  m_view_model->set_value(*m_curveInterpretationParam, interpretation);
  onParameterChanged();
}

void MainWindow::onApplyClicked() {
  // Save current profile to disk
  m_view_model->apply(*m_store);
}

// ----------------------------------------------------------------------------

void MainWindow::connectControls() {
  connect(m_ui->pushButton, &QPushButton::clicked, this,
          &MainWindow::onApplyClicked);

  connect(m_ui->curveSelector, &QListWidget::currentRowChanged, this,
          &MainWindow::onCurveSelectionChanged);

  connectCurveInterpretation();
  connectFooterControls();
}

void MainWindow::connectCurveInterpretation() {
  connect(m_ui->curveInterpretationGainRadioButton, &QRadioButton::clicked,
          this, [&](bool checked) {
            onCurveInterpretation(checked, CurveInterpretation::kGain);
          });
  connect(m_ui->curveInterpretationSensitivityRadioButton,
          &QRadioButton::clicked, this, [&](bool checked) {
            onCurveInterpretation(checked, CurveInterpretation::kSensitivity);
          });
}

template <bool triggersRedraw, typename SpinBox, typename Value>
void MainWindow::connectFooterSpinBox(auto& label, SpinBox& spinBox,
                                      Param<Value>& param) {
  syncParamToUi(label, spinBox, param);
  connect(&spinBox, &SpinBox::valueChanged, this, [&](Value value) {
    m_view_model->set_value(param, value);
    if (triggersRedraw) onParameterChanged();
  });
}

template <typename CheckBox, typename SpinBox, typename Value>
void MainWindow::connectFooterFilterParams(CheckBox& checkBox,
                                           Param<bool>& checkBoxParam,
                                           SpinBox& spinbox,
                                           Param<Value>& spinBoxParam) {
  checkBox.setText(QString::fromUtf8(checkBoxParam.name()) + ":");
  checkBox.setCheckState(checkBoxParam.value() ? Qt::Checked : Qt::Unchecked);
  connect(&checkBox, &CheckBox::checkStateChanged, this,
          [&](Qt::CheckState checkState) {
            m_view_model->set_value(checkBoxParam, Qt::Unchecked != checkState);
          });

  spinbox.setMinimum(spinBoxParam.min());
  spinbox.setMaximum(spinBoxParam.max());
  spinbox.setValue(spinBoxParam.value());
  connect(&spinbox, &SpinBox::valueChanged, this,
          [&](Value value) { m_view_model->set_value(spinBoxParam, value); });
}

void MainWindow::connectFooterControls() {
  connectFooterSpinBox<true>(*m_ui->sensitivityLabel,
                             *m_ui->sensitivityDoubleSpinBox,
                             m_view_model->sensitivity_param());

  connectFooterSpinBox<false>(*m_ui->dpiLabel, *m_ui->dpiSpinBox,
                              m_view_model->dpi_param());

  connectFooterSpinBox<false>(*m_ui->anisotropyLabel,
                              *m_ui->anisotropyDoubleSpinBox,
                              m_view_model->anisotropy_param());

  connectFooterSpinBox<false>(*m_ui->rotationLabel,
                              *m_ui->rotationDoubleSpinBox,
                              m_view_model->rotation_param());

  connectFooterFilterParams(*m_ui->filterSpeedCheckBox,
                            m_view_model->filter_speed_param(),
                            *m_ui->speedFilterHalflifeDoubleSpinBox,
                            m_view_model->speed_filter_halflife_param());

  connectFooterFilterParams(*m_ui->filterScaleCheckBox,
                            m_view_model->filter_scale_param(),
                            *m_ui->scaleFilterHalflifeDoubleSpinBox,
                            m_view_model->scale_filter_halflife_param());

  connectFooterFilterParams(*m_ui->filterOutputCheckBox,
                            m_view_model->filter_output_param(),
                            *m_ui->outputFilterHalflifeDoubleSpinBox,
                            m_view_model->output_filter_halflife_param());
}

void MainWindow::populateCurveSelector() {
  applyGeneratedCss(*m_ui->curveSelector, curveSelectorCssTemplate);

  // For now, we manually iterate the known curve types.
  // When we add more curves, we add them here and to the enum.
  m_ui->curveSelector->clear();

  // Add each curve type to the list widget
  // The row index corresponds to the enum value
  m_ui->curveSelector->addItem(
      QString::fromUtf8(to_string(CurveType::kSynchronous)));

  // Future curves would be added here:
  // m_ui->curveSelector->addItem(
  //     QString::fromUtf8(to_string(CurveType::kLinear)));
}

void MainWindow::selectConfiguredCurve() {
  auto selected = static_cast<int>(m_view_model->selected_curve());
  m_ui->curveSelector->setCurrentRow(selected);
}

void MainWindow::constrainConfigHeight() {
  setListMinHeight(*m_ui->curveConfig, minVisibleParameters);
}

void MainWindow::rebuildParameterWidgets(CurveType curve) {
  clearParameterWidgets();

  // Use the ViewModel to iterate all parameters for this curve
  m_view_model->for_each_curve_param(curve, [this](auto& param) {
    // Check if this is a Param<double> - those get spinbox widgets
    using ParamType = std::decay_t<decltype(param)>;

    if constexpr (std::is_same_v<ParamType, Param<double>>) {
      auto* widget = new CurveParameter(m_view_model, param);

      // Connect the widget's changed signal to our handler
      connect(widget, &CurveParameter::valueChanged, this,
              &MainWindow::onParameterChanged);

      // Add to the list widget
      auto* item = new QListWidgetItem(m_ui->curveConfig);
      item->setSizeHint(widget->sizeHint());
      m_ui->curveConfig->addItem(item);
      m_ui->curveConfig->setItemWidget(item, widget);

      m_parameter_widgets.push_back(widget);
    } else if constexpr (std::is_same_v<ParamType,
                                        Param<CurveInterpretation>>) {
      m_curveInterpretationParam = &param;
      switch (param.value()) {
        case CurveInterpretation::kGain:
          m_ui->curveInterpretationGainRadioButton->setChecked(true);
          m_ui->curveInterpretationSensitivityRadioButton->setChecked(false);
          break;

        case CurveInterpretation::kSensitivity:
          m_ui->curveInterpretationGainRadioButton->setChecked(false);
          m_ui->curveInterpretationSensitivityRadioButton->setChecked(true);
          break;
      }
    }
  });
}

void MainWindow::clearParameterWidgets() {
  // The QListWidget owns the items and widgets, so clearing it deletes them
  m_ui->curveConfig->clear();
  m_parameter_widgets.clear();
}

void MainWindow::updateCurveDisplay() {
  m_ui->curve_editor->setSpline(m_view_model->create_spline(),
                                m_curveInterpretationParam->value());
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

void MainWindow::applyGeneratedCss(QWidget& widget,
                                   std::string_view cssTemplate) {
  const auto standardPalette = QApplication::palette();
  const auto window = standardPalette.color(QPalette::Window);
  const auto windowText = standardPalette.color(QPalette::WindowText);
  const auto highlight = standardPalette.color(QPalette::Highlight);
  const auto highlightText = standardPalette.color(QPalette::HighlightedText);

  const auto darkBackground = window.darker(200);
  const auto hoverBackground = darkBackground.lighter(150);

  const auto replacedTabViewCss = QString{cssTemplate.data()}.arg(
      darkBackground.name(), windowText.name(), hoverBackground.name(),
      highlight.name(), highlightText.name());

  widget.setStyleSheet(replacedTabViewCss);
}

const std::string_view MainWindow::curveSelectorCssTemplate = R"(
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
