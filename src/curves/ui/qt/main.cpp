#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>

// Include the headers for all our C++ types
#include "CurveParameterModel.hpp"
#include "CurveTypeModel.hpp"
#include "EditorPresenter.hpp"

int main(int argc, char* argv[]) {
  qputenv("QT_LOGGING_RULES", "qt.qml*=true");

  QGuiApplication app(argc, argv);

  // Set an icon (paths in .qrc are prefixed with :/)
  // app.setWindowIcon(QIcon(":/app.ico"));

  // --- 1. Register All C++ Types with QML ---
  // This makes QML aware of the *class types*.
  // We are registering them under the module "dev.mouse.curves" version 1.0
  // This is what allows 'import dev.mouse.curves 1.0' to work in QML.
  // qmlRegisterModule("dev.mouse.curves", 1, 0);

  // const char* uri = "dev.mouse.curves";
  // qmlRegisterType<EditorPresenter>(uri, 1, 0, "EditorPresenter");
  // qmlRegisterType<CurveTypeModel>(uri, 1, 0, "CurveTypeModel");
  // qmlRegisterType<CurveParameterModel>(uri, 1, 0, "CurveParameterModel");

  // --- 2. Create the QML Engine ---
  QQmlApplicationEngine engine;

  // Add explicit import path
  QString pluginPath = QCoreApplication::applicationDirPath() + "/plugins";
  engine.addImportPath(pluginPath);
  qDebug() << "Plugin path:" << pluginPath;

  // --- 3. Create the Main Presenter Instance ---
  // This is the single C++ object that will manage the application's state.
  EditorPresenter editorPresenter(&engine);

  // --- 4. Expose the Presenter Instance to QML's Root Context ---
  // This makes the 'editorPresenter' object globally available in QML
  // by that name. This is how main.qml can access
  // 'editorPresenter.curveTypeModel' or call
  // 'editorPresenter.onCurveTypeSelected()'.
  engine.rootContext()->setContextProperty("editorPresenter", &editorPresenter);

  // --- 5. Load the Main QML File ---
  // This must be done AFTER registering types and context properties.
  const QUrl url(QStringLiteral("qrc:/main.qml"));
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &app,
      [url](QObject* obj, const QUrl& objUrl) {
        if (!obj && url == objUrl) QCoreApplication::exit(-1);
      },
      Qt::QueuedConnection);

  qDebug() << "About to load QML...";
  engine.load(url);
  qDebug() << "QML loaded successfully";

  // --- 6. Run the Application ---
  return app.exec();
}
