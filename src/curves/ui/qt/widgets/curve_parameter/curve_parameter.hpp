// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Widget for editing a single curve parameter.

  CurveParameter displays a labeled QDoubleSpinBox for editing a Param<double>.
  It reads min/max/value from the Param and writes changes through the
  ViewModel.

  \copyright Copyright (C) 2025 Frank Secilia
*/
#ifndef CURVE_PARAMETER_HPP
#define CURVE_PARAMETER_HPP

#include <curves/config/param.hpp>
#include <curves/ui/model/view_model.hpp>
#include <QWidget>
#include <memory>

namespace Ui {
class CurveParameter;
}

void syncParamToUi(auto& label, auto& spinbox, const auto& param) {
  label.setText(QString::fromUtf8(param.name()) + ":");
  spinbox.setMinimum(param.min());
  spinbox.setMaximum(param.max());
  spinbox.setValue(param.value());
}

class CurveParameter : public QWidget {
  Q_OBJECT

 public:
  /*!
    \brief Constructs a parameter editor widget.

    \param viewmodel ViewModel to write changes through.
    \param param Parameter to display and edit (must outlive this widget).
    \param parent Optional parent widget.
  */
  explicit CurveParameter(std::shared_ptr<curves::ViewModel> viewmodel,
                          curves::Param<double>& param,
                          QWidget* parent = nullptr);

  ~CurveParameter() override;

  auto parameterIndex() const -> int;
  auto setParameterIndex(int index) -> void;

  auto setLabelText(const QString& label) -> void;

  auto spinBoxValue() const -> double;
  auto setSpinBoxValue(double value) -> void;

 signals:
  void valueChanged() const;

 private slots:
  void onSpinBoxValueChanged(double value) const;

 private:
  std::unique_ptr<Ui::CurveParameter> ui;
  std::shared_ptr<curves::ViewModel> m_view_model;
  curves::Param<double>* m_param;
};

#endif  // CURVE_PARAMETER_HPP
