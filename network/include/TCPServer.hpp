#pragma once

#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

class TCPServer {

private:
  int server_fd;
  uint16_t port;
  sockaddr_in6 address{};

public:
  TCPServer(uint16_t port);
  int init();
  bool sendToClient(int client_fd, const char *msg);
  int configureSocket();
};
