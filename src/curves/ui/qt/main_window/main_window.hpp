// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Application's main window.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <QMainWindow>
#include <memory>  // For std::unique_ptr

// Forward declaration of the class that uic will generate from your .ui file.
// This prevents us from having to include the generated header here.
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
  // This macro is REQUIRED for any class that defines
  // its own signals or slots.
  Q_OBJECT

 public:
  // We'll use a standard `QWidget *parent = nullptr` constructor.
  // `explicit` prevents accidental conversions.
  explicit MainWindow(QWidget* parent = nullptr);

  // We MUST declare a destructor, but it can be defaulted.
  // This is a C++ requirement for using std::unique_ptr
  // with a forward-declared type (the "pimpl" idiom).
  ~MainWindow() override;

 private:
  // This is the magic pointer.
  // It will hold all the widgets you defined in Designer.
  // We use Google C++ style (trailing underscore for private members)
  // and std::unique_ptr for automatic memory management.
  std::unique_ptr<Ui::MainWindow> ui_;
};
