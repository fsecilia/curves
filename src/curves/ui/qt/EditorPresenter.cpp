#include "EditorPresenter.hpp"
#include "CurveParameterModel.hpp"
#include "CurveTypeModel.hpp"
#include <QDebug>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QQmlContext>

EditorPresenter::EditorPresenter(QQmlEngine* engine, QObject* parent)
    : QObject(parent), m_engine{engine} {
  // Instantiate the models
  // This class "owns" them, so it's responsible for their lifetime.
  // We pass 'this' as the parent so they are
  // auto-destroyed when the presenter is.
  m_curveTypeModel = new CurveTypeModel(this);
  m_curveParameterModel = new CurveParameterModel(this);

  updateCurveList();

  // Set the initial state
  // Select the first curve by default for now. Should come from driver.
  onCurveTypeSelected(0);
}

// --- Property Getters ---

auto EditorPresenter::curveTypeModel() const -> CurveTypeModel* {
  return m_curveTypeModel;
}

auto EditorPresenter::curveParameterModel() const -> CurveParameterModel* {
  return m_curveParameterModel;
}

// --- QML-Invokable Functions ---

/**
 * @brief This is the main logic function.
 * QML calls this when the user clicks a tab in the first column.
 * This function is responsible for updating the second column
  // Populate the CurveTypeModel with initial data
  // These will need to be laded from the driver, or read from the sysfs.
  m_curveTypeModel->populate({"Linear", "Classic", "Power"});
* (CurveParameterModel) and the third column (the CurveView).
 */
void EditorPresenter::onCurveTypeSelected(int index) {
  // 1. Get the selected curve name from the model
  //    We need to get the data from the model using its index and role.
  auto model_index = m_curveTypeModel->index(index, 0);
  QString curve_name =
      m_curveTypeModel->data(model_index, CurveTypeModel::DisplayNameRole)
          .toString();

  qDebug() << "Curve selected:" << curve_name;

  // 2. Create the list of parameters for the selected curve
  //    In a real app, you'd fetch this from your data objects.
  //    Here, we'll create mock data.
  QList<CurveParameter> params;

  if (curve_name == "Linear") {
    params.append({"Sensitivity", 1.0, "double"});
    params.append({"Offset", 0.0, "double"});
  } else if (curve_name == "Classic") {
    params.append({"Sensitivity", 1.2, "double"});
    params.append({"Acceleration", 0.02, "double"});
    params.append({"Offset", 0.0, "double"});
    params.append({"Limit", 2.0, "double"});
  } else if (curve_name == "Power") {
    params.append({"Sensitivity", 1.0, "double"});
    params.append({"Exponent", 2.2, "double"});
  } else {
    // Handle unknown or empty state
  }

  // 3. Update the CurveParameterModel
  //    This will cause the ListView in QML to
  //    automatically redraw itself.
  m_curveParameterModel->setParameters(params);

  // 4. (Future) Update the CurveView
  //    You would now tell your CurvePresenter to load
  //    the data for the selected curve.
  //
  //    e.g., m_curvePresenter->loadCurve(m_allCurveData.at(index));
}

auto EditorPresenter::updateCurveList() -> void {
  // currently, this gets a list of names and a default font, then
  // figures they'll match what's in the widgets. It should check the widgets.

  // Get your list of names
  // These will need to be laded from the driver, or read from the sysfs.
  QStringList curveNames = {"Linear", "Classic", "Power"};
  m_curveTypeModel->populate(curveNames);

  // Get the font
  // This uses the default app font is usually fine, but should get the font
  // from the widgets we're sizing.
  QFont font = QGuiApplication::font();
  QFontMetrics fm(font);

  // Find the widest string
  int maxWidth = 0;
  for (const QString& name : curveNames) {
    // 'horizontalAdvance' is the pixel width
    maxWidth = std::max(maxWidth, fm.horizontalAdvance(name));
  }

  // Add padding (e.g., 20px on each side + scrollbar)
  int finalWidth = maxWidth + 50;

  // Expose it to QML
  m_engine->rootContext()->setContextProperty("curveListWidth", finalWidth);
}
