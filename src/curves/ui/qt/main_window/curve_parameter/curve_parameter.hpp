#ifndef CURVE_PARAMETER_HPP
#define CURVE_PARAMETER_HPP

#include <QWidget>
#include <memory>

namespace Ui {
class CurveParameter;
}

class CurveParameter : public QWidget {
  Q_OBJECT

 public:
  explicit CurveParameter(QWidget* parent = nullptr);
  ~CurveParameter();

  auto parameterIndex() const -> int;
  auto setParameterIndex(int index) -> void;

  auto setLabelText(const QString& label) -> void;

  auto spinBoxValue() const -> double;
  auto setSpinBoxValue(double value) -> void;

 signals:
  void spinBoxValueChanged(int parameter_index, double value) const;

 private slots:
  void onSpinBoxValueChanged(double value) const;

 private:
  std::unique_ptr<Ui::CurveParameter> ui;
  int m_parameter_index{};
};

#endif  // CURVE_PARAMETER_HPP
