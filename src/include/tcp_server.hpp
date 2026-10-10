#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

class StoreController;

class TCPServer {

private:
  int server_fd;
  uint16_t port;
  sockaddr_in6 address{};
  StoreController *store_controller;

public:
  TCPServer(uint16_t port, StoreController *store_controller);
  int init();
  bool sendToClient(const int client_fd, const char *msg);
  int configureSocket();
};
