#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

class TCPClient {
private:
  int client_fd;
  uint16_t port;
  sockaddr_in6 server{};

public:
  TCPClient(uint16_t port);
  int init();
};
