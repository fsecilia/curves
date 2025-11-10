#ifndef CURVE_PARAMETER_MODEL_H
#define CURVE_PARAMETER_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QVariant>

// @uri dev.mouse.curves
// Placeholder struct for a single parameter.
// Your "real" class would be more complex.
struct CurveParameter {
  QString label;
  QVariant value;
  QString type;  // "double", "int", "bool"
};

// This model holds the list for the second column (the parameters).
class CurveParameterModel : public QAbstractListModel {
  Q_OBJECT

 public:
  // Define the roles for QML. A delegate can ask for
  // 'model.label', 'model.value', 'model.type', etc.
  enum Roles {
    LabelRole = Qt::UserRole + 1,
    ValueRole,
    TypeRole
    // You could add minRole, maxRole, etc.
  };

  explicit CurveParameterModel(QObject* parent = nullptr);

  // --- QAbstractListModel Overrides ---
  auto rowCount(const QModelIndex& parent = QModelIndex()) const
      -> int override;

  auto data(const QModelIndex& index, int role = Qt::DisplayRole) const
      -> QVariant override;

  auto roleNames() const -> QHash<int, QByteArray> override;

  // --- Public API ---
  // EditorPresenter calls this to update the list.
  auto setParameters(const QList<CurveParameter>& params) -> void;

 private:
  QList<CurveParameter> m_parameters;
};

#endif  // CURVE_PARAMETER_MODEL_H
