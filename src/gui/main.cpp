#include "gui/main_window.h"
#include <QApplication>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  // Set application style
  app.setStyle("Fusion");

  // Set application info
  QApplication::setApplicationName("科研审稿系统");
  QApplication::setOrganizationName("Review System");
  QApplication::setApplicationVersion("1.0");

  gui::MainWindow window;
  window.show();

  return app.exec();
}
