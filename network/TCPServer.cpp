#include "TCPServer.hpp"
#include <cstring>
#include <iostream>
#include <sys/socket.h>

TCPServer::TCPServer(uint16_t port) : port(port) {

  server_fd = socket(AF_INET6, SOCK_STREAM, 0);

  std::memset(&address, 0, sizeof(address));

  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = htons(port);
}

int TCPServer::init() {

  if (server_fd < 0) {
    perror("socket");
    return 1;
  }

  if (bind(server_fd, (sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
    close(server_fd);
    return 1;
  }

  if (listen(server_fd, 5) < 0) {
    perror("listen");
    close(server_fd);
    return 1;
  };

  int client_fd = accept(server_fd, nullptr, nullptr);
  if (client_fd < 0) {
    perror("accept");
    close(server_fd);
    return 1;
  }

  const char *message = "Hello from server\n";
  const int len = std::strlen(message);

  // Data is not guarenteed to be sent at once. Can be sent in any increment of
  // the original value. Must be explicitly handled
  ssize_t total_sent = 0;
  while (total_sent < len) {
    ssize_t n = send(client_fd, message + total_sent, len - total_sent, 0);

    if (n <= 0) {
      perror("send");
      break;
    }

    total_sent += n;
  }
  send(client_fd, message, std::strlen(message), 0);

  close(client_fd);
  close(server_fd);

  return 0;
}
