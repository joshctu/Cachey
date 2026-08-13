#include "store.hpp"
#include <filesystem>

#include "gui.hpp"
#include <QApplication>
#include <exceptions.hpp>

/**
 * @brief Sets up database and inits first logfile if not exists
 *
 *
 */
int main(int argc, char *argv[]) {

  QApplication app(argc, argv);
  // Init log dir if not exists
  std::filesystem::create_directories("./database/wal/logs");

  KV_Store store;
  StoreController storeController(store);
  MainWindow mainWindow(storeController);

  mainWindow.show();

  return app.exec();
}
