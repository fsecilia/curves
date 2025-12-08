// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Application's main window.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <QMainWindow>
#include <memory>

struct curves_spline_table;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);

  ~MainWindow() override;

  auto prepopulateCurveParameterWidgets(int numWidgets) -> void;
  auto setSpline(std::shared_ptr<const curves_spline_table> spline_table)
      -> void;

 private slots:
  void onSpinBoxValueChanged(int parameter_index, double value) const;

 private:
  std::unique_ptr<Ui::MainWindow> m_ui;
};
