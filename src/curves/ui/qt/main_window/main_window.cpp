#include "main_window.hpp"
#include "curve_parameter/curve_parameter.hpp"
#include "ui_main_window.h"

#include <iostream>

// Constructor
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_ui(std::make_unique<Ui::MainWindow>()) {
  m_ui->setupUi(this);
}

MainWindow::~MainWindow() = default;

auto MainWindow::prepopulateCurveParameterWidgets(int numWidgets) -> void {
  for (int i = 0; i < numWidgets; ++i) {
    auto curve_parameter_unique = std::make_unique<CurveParameter>();
    curve_parameter_unique->setParameterIndex(i);

    auto list_item_unique =
        std::make_unique<QListWidgetItem>(m_ui->curve_parameters);
    list_item_unique->setSizeHint(curve_parameter_unique->sizeHint());
    m_ui->curve_parameters->addItem(list_item_unique.get());
    auto list_item = list_item_unique.release();

    m_ui->curve_parameters->setItemWidget(list_item,
                                          curve_parameter_unique.get());
    auto curve_parameter = curve_parameter_unique.release();

    connect(curve_parameter, &CurveParameter::spinBoxValueChanged, this,
            &MainWindow::onSpinBoxValueChanged);
  }
}

void MainWindow::onSpinBoxValueChanged(int parameter_index,
                                       double value) const {
  std::cout << "MainWindow::onSpinBoxValueChanged(" << parameter_index << ", "
            << value << ")" << std::endl;
}
