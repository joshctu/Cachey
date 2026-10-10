#include "gui.hpp"
#include "store.hpp"
#include "tcp_server.hpp"
#include <QApplication>
#include <QtGlobal>
#include <exceptions.hpp>
#include <thread>

/**
 * @brief Sets up database and inits first logfile if not exists
 *
 *
 */
int main(int argc, char *argv[]) {

  QApplication app(argc, argv);

  ProjectConfig config;

  KV_Store store(config);
  StoreController storeController(&store);
  MainWindow mainWindow(&storeController);

  TCPServer server(8080, &storeController);
  std::thread server_thread(&TCPServer::init, &server);

  mainWindow.show();

  return app.exec();
}
