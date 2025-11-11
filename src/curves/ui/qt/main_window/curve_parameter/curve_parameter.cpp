#include "curve_parameter.hpp"
#include "ui_curve_parameter.h"

CurveParameter::CurveParameter(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::CurveParameter>()) {
  ui->setupUi(this);
  connect(ui->doubleSpinBox, &QDoubleSpinBox::valueChanged, this,
          &CurveParameter::onSpinBoxValueChanged);
}

CurveParameter::~CurveParameter() = default;

auto CurveParameter::parameterIndex() const -> int { return m_parameter_index; }

auto CurveParameter::setParameterIndex(int index) -> void {
  m_parameter_index = index;
}

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
  emit spinBoxValueChanged(m_parameter_index, value);
}
