#include "store.hpp"
#include <filesystem>

#include "gui.hpp"
#include <QApplication>
#include <QtGlobal>
#include <exceptions.hpp>
#include <iostream>

/**
 * @brief Sets up database and inits first logfile if not exists
 *
 *
 */
int main(int argc, char *argv[]) {
  std::string version = QT_VERSION_STR;
  std::cout << version << std::endl;

  QApplication app(argc, argv);
  // Init log dir if not exists
  std::filesystem::create_directories("./database/wal/segments");

  KV_Store store;
  StoreController storeController(&store);
  MainWindow mainWindow(storeController);

  mainWindow.show();

  return app.exec();
}
