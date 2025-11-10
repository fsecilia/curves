#ifndef CURVE_TYPE_MODEL_H
#define CURVE_TYPE_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>

// @uri dev.mouse.curves
// This model holds the list for the first column (the "tabs").
// It's a QAbstractListModel, which is the standard way
// to provide dynamic lists to QML.
class CurveTypeModel : public QAbstractListModel {
  Q_OBJECT

 public:
  // Define the "roles" that QML can ask for.
  // A QML delegate can say 'model.displayName' and this
  // enum connects that name to a data role.
  enum Roles { DisplayNameRole = Qt::UserRole + 1, CurveIdRole };

  explicit CurveTypeModel(QObject* parent = nullptr);

  // --- QAbstractListModel Overrides ---
  auto rowCount(const QModelIndex& parent = QModelIndex()) const
      -> int override;

  auto data(const QModelIndex& index, int role = Qt::DisplayRole) const
      -> QVariant override;

  auto roleNames() const -> QHash<int, QByteArray> override;

  // --- Public API ---
  // You would call this from your EditorPresenter to fill the model.
  auto populate(const QList<QString>& curveNames) -> void;

 private:
  // The data for this simple model is just a list of strings.
  QStringList m_curveNames;
};

#endif  // CURVE_TYPE_MODEL_H
