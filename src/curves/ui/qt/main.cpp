#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>

auto main(int argc, char* argv[]) -> int {
  QGuiApplication app(argc, argv);

  // Set an icon (paths in .qrc are prefixed with :/)
  // app.setWindowIcon(QIcon(":/app.ico"));

  QQmlApplicationEngine engine;

  // Load the main QML file from the Qt Resource System
  const QUrl url(QStringLiteral("qrc:/main.qml"));

  // Connect a lambda to the objectCreated signal to check if loading failed
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &app,
      [url](QObject* obj, const QUrl& objUrl) {
        if (!obj && url == objUrl) {
          QCoreApplication::exit(-1);
        }
      },
      Qt::QueuedConnection);

  engine.load(url);

  return app.exec();
}
