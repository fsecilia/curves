// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "curve_parameter.hpp"
#include "ui_curve_parameter.h"
#include <utility>

CurveParameter::CurveParameter(std::shared_ptr<curves::ViewModel> view_model,
                               curves::Param<double>& param, QWidget* parent)
    : QWidget(parent),
      ui(std::make_unique<Ui::CurveParameter>()),
      m_view_model{std::move(view_model)},
      m_param{&param} {
  ui->setupUi(this);

  setLabelText(QString::fromUtf8(m_param->name()) + ":");

  ui->doubleSpinBox->setMinimum(m_param->min());
  ui->doubleSpinBox->setMaximum(m_param->max());
  setSpinBoxValue(m_param->value());

  connect(ui->doubleSpinBox, &QDoubleSpinBox::valueChanged, this,
          &CurveParameter::onSpinBoxValueChanged);
}

CurveParameter::~CurveParameter() = default;

auto CurveParameter::setLabelText(const QString& label) -> void {
  ui->label->setText(label);
}

auto CurveParameter::spinBoxValue() const -> double {
  return ui->doubleSpinBox->value();
}

auto CurveParameter::setSpinBoxValue(double value) -> void {
  ui->doubleSpinBox->setValue(value);
}

void CurveParameter::onSpinBoxValueChanged(double value) const {
  // Write through the ViewModel.
  m_view_model->set_value(*m_param, value);

  // Notify that we changed. MainWindow catches this to redraw the curve.
  emit valueChanged();
}
