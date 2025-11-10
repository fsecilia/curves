#ifndef EDITOR_PRESENTER_H
#define EDITOR_PRESENTER_H

#include <QObject>
#include <QQmlEngine>

// Forward declare the models to avoid circular includes
class CurveTypeModel;
class CurveParameterModel;

// @uri dev.mouse.curves
// This is the main C++ QObject that QML will interact with.
// You register this one object in your main.cpp as a context property.
class EditorPresenter : public QObject {
  Q_OBJECT

  // Expose the two models to QML as properties.
  // QML will access them as 'editorPresenter.curveTypeModel' and
  // 'editorPresenter.curveParameterModel'.
  Q_PROPERTY(CurveTypeModel* curveTypeModel READ curveTypeModel CONSTANT)
  Q_PROPERTY(CurveParameterModel* curveParameterModel READ curveParameterModel
                 CONSTANT)

 public:
  explicit EditorPresenter(QQmlEngine* engine, QObject* parent = nullptr);

  // --- Property Getters ---
  auto curveTypeModel() const -> CurveTypeModel*;
  auto curveParameterModel() const -> CurveParameterModel*;

  // --- QML-Invokable Functions ---
  // This is the function the TabBar will call when a new curve is clicked.
  Q_INVOKABLE void onCurveTypeSelected(int index);

 private:
  QQmlEngine* m_engine;

  // This class owns the models
  CurveTypeModel* m_curveTypeModel;
  CurveParameterModel* m_curveParameterModel;

  // This would hold your actual data, e.g.:
  // QVector<MyCurveData> m_allCurveData;

  auto updateCurveList() -> void;
};

#endif  // EDITOR_PRESENTER_H
