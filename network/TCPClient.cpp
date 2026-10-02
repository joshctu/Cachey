#include "TCPClient.hpp"
#include <arpa/inet.h>
#include <iostream>
#include <sys/socket.h>

TCPClient::TCPClient(uint16_t port) : port(port) {
  client_fd = socket(AF_INET6, SOCK_STREAM, 0);

  server.sin6_family = AF_INET6;
  server.sin6_port = htons(port);

  inet_pton(AF_INET6, "::1", &server.sin6_addr);
};

int TCPClient::init() {

  if (client_fd < 0) {
    perror("socket");
    close(client_fd);
    return 1;
  }

  if (connect(client_fd, reinterpret_cast<sockaddr *>(&server),
              sizeof(server)) < 0) {
    perror("connect");
    close(client_fd);
    return 1;
  }

  char buffer[128];

  // This could recieve fewer bytes than requested
  ssize_t n = read(client_fd, buffer, sizeof(buffer));

  if (n < 0 && errno == EINTR) {
    ssize_t n = read(client_fd, buffer, sizeof(buffer));
  }

  std::cout << "Message recieved: " << buffer;

  close(client_fd);
  return 0;
}
