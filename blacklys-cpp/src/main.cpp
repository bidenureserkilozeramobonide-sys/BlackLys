#include "app/MainWindow.h"
#include "database/Database.h"
#include <QApplication>
#include <QDir>
#include <QFile>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("BlackLys");
  app.setApplicationVersion("1.0.0");
  app.setOrganizationName("BlackLys");

  // Initialize database
  if (!Database::instance().initialize()) {
    qWarning() << "[BlackLys] Database initialization failed!";
  }

  // Load QSS dark theme — prefer filesystem, fallback to resource
  QString qssPath =
      QCoreApplication::applicationDirPath() + "/../src/resources/style.qss";
  QFile styleFile(qssPath);
  if (!styleFile.exists()) {
    // Fallback: try resource
    styleFile.setFileName(":/resources/style.qss");
  }
  if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    app.setStyleSheet(styleFile.readAll());
    styleFile.close();
    qDebug() << "[BlackLys] Loaded theme from:" << styleFile.fileName();
  }

  MainWindow window;
  window.showMaximized();

  return app.exec();
}
