// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Application's main window.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/config/profile_store.hpp>
#include <curves/ui/model/view_model.hpp>
#include <QListWidget>
#include <QMainWindow>
#include <memory>
#include <string_view>

struct curves_spline;

namespace Ui {
class MainWindow;
}

class CurveParameter;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(std::shared_ptr<curves::ViewModel> view_model,
                      std::shared_ptr<curves::ProfileStore> store,
                      QWidget* parent = nullptr);

  ~MainWindow() override;

 private slots:
  //! Called when a parameter value changes in any widget.
  void onParameterChanged();

  //! Called when user selects a different curve in the curve list.
  void onCurveSelectionChanged(int index);

  //! Called when user clicks the Apply button.
  void onApplyClicked();

 private:
  std::unique_ptr<Ui::MainWindow> m_ui;
  std::shared_ptr<curves::ViewModel> m_view_model;
  std::shared_ptr<curves::ProfileStore> m_store;
  std::vector<CurveParameter*> m_parameter_widgets;
  static const std::string_view tabViewCss;

  //! Wires up control signals.
  void wireUpControls();

  template <bool triggersRedraw, typename SpinBox, typename Value>
  void connectFooterSpinBox(auto& label, SpinBox& spinBox,
                            curves::Param<Value>& param);
  template <typename CheckBox, typename SpinBox, typename Value>
  void connectFooterFilterParams(CheckBox& checkbox,
                                 curves::Param<bool>& checkboxParam,
                                 SpinBox& spinbox,
                                 curves::Param<Value>& spinBoxParam);
  void connectFooterControls();

  //! Populates curve selector list from CurveType enum values.
  void populateCurveSelector();

  //! Rebuilds the parameter widget list for the given curve type.
  void rebuildParameterWidgets(curves::CurveType curve);

  //! Clears all parameter widgets from the list.
  void clearParameterWidgets();

  //! Creates the spline from current parameters and updates the graph.
  void updateCurveDisplay();

  //! Applies generated css from tabViewCss and system palette.
  void applyTabViewCss(QWidget& widget);

  //! Sets list min height to multiple of the height size hint.
  void setListMinHeight(QListWidget& list, int minVisibleItems);
};
