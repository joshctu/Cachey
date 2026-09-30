#include "TCPClient.hpp"
#include "TCPServer.hpp"
#include "gui.hpp"
#include "store.hpp"
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

  TCPServer server(8080);
  std::thread server_thread(&TCPServer::init, &server);

  TCPClient client(8080);
  std::thread client_thread(&TCPClient::init, &client);

  QApplication app(argc, argv);

  ProjectConfig config;

  KV_Store store(config);
  StoreController storeController(&store);
  MainWindow mainWindow(&storeController);

  mainWindow.show();

  return app.exec();
}
