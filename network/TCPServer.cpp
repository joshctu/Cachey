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

  if (bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) <
      0) {
    perror("bind");
    close(server_fd);
    return 1;
  }

  if (listen(server_fd, 5) < 0) {
    perror("listen");
    close(server_fd);
    return 1;
  };

  while (true) {
    int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
      perror("accept");
      close(server_fd);
      return 1;
    }

    uint32_t msg_len;
    // Gonna try out waitall
    recv(client_fd, &msg_len, sizeof(msg_len), MSG_WAITALL);
    msg_len = ntohl(msg_len);

    char *buffer = new char[msg_len];
    recv(client_fd, buffer, msg_len, MSG_WAITALL);

    uint8_t operation;
    uint32_t key_size;

    uint32_t bytes_copied = 0;

    std::memcpy(&operation, buffer, sizeof(uint8_t));
    bytes_copied += 1;

    std::memcpy(&key_size, buffer + 1, sizeof(uint32_t));
    key_size = ntohl(key_size);
    bytes_copied += 4;

    char *key = new char[key_size + 1];
    std::memcpy(key, buffer + 5, key_size);
    key[key_size + 1] = '\0';

    bytes_copied += key_size;

    std::string result = std::to_string(operation) + "/" +
                         std::to_string(key_size) + "/" + key + "/";

    if (msg_len > bytes_copied) {
      uint32_t value_size;

      std::memcpy(&value_size, buffer + (5 + key_size), sizeof(uint32_t));
      bytes_copied += 4;
      value_size = ntohl(value_size);

      char *value = new char[value_size + 1];
      std::memcpy(value, buffer + (5 + key_size + 4), value_size);
      value[value_size + 1] = '\0';
      bytes_copied += value_size;

      result += (std::to_string(value_size) + "/" + value);

      // if (!success)
      //  std::cout << "We failed to send message back to client" << '\n';
    }

    bool success = sendToClient(client_fd, result.c_str());
    if (!success)
      std::cout << "We failed to send the result back to the client";
    return 0;
  }
  close(server_fd);
  return 0;
}

bool TCPServer::sendToClient(int client_fd, const char *msg) {
  int len = std::strlen(msg);

  ssize_t total_sent = 0;
  while (total_sent <= len) {
    ssize_t n = send(client_fd, msg + total_sent, len - total_sent, 0);

    if (n <= 0) {
      perror("send");
      return false;
    }

    total_sent += n;
  }

  return true;
}

int TCPServer::configureSocket() {}
