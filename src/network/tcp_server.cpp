#include "tcp_server.hpp"
#include <cstring>
#include <iostream>
#include <store_controller.hpp>
#include <sys/socket.h>

TCPServer::TCPServer(uint16_t port, StoreController *store_controller)
    : port(port), store_controller(store_controller) {

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

    std::vector<char> buffer;
    buffer.resize(msg_len);

    recv(client_fd, buffer.data(), msg_len, MSG_WAITALL);

    uint8_t operation;
    uint32_t key_size;

    size_t offset = 0;

    std::memcpy(&operation, buffer.data() + offset, sizeof(uint8_t));
    offset += 1;

    std::memcpy(&key_size, buffer.data() + offset, sizeof(uint32_t));
    key_size = ntohl(key_size);
    offset += 4;

    std::vector<char> key;
    key.resize(key_size + 1);

    std::memcpy(key.data(), buffer.data() + offset, key_size);
    key[key_size] = '\0';
    offset += key_size;

    std::string key_string{key.begin(), key.end()};

    if (operation == 0x20 && msg_len > offset) {
      uint32_t value_size;

      std::memcpy(&value_size, buffer.data() + offset, sizeof(uint32_t));
      offset += 4;
      value_size = ntohl(value_size);

      std::vector<char> value;
      value.resize(value_size + 1);
      std::memcpy(value.data(), buffer.data() + offset, value_size);
      value[value_size] = '\0';
      offset += value_size;

      std::string value_string{value.begin(), value.end()};

      SetResult set_result =
          store_controller->handleSetKeyValue(key_string, value_string);

      if (set_result.success) {
        std::string msg = std::to_string(operation) + "/" + key_string.data() +
                          "/" + value_string.data();

        bool set_result = sendToClient(client_fd, msg.data());
        if (!set_result)
          std::cout << "Failed to send set result to client" << "\n";
      } else {
        std::cout << "Set handler failed" << "\n";
      }
    }

    else if (operation == 0x10) {

      GetResult get_result = store_controller->handleGetValue(key_string);

      if (get_result.success) {
        std::string msg = std::to_string(operation) + "/" + key_string.data() +
                          "/" + get_result.value.data();

        bool get_result = sendToClient(client_fd, msg.data());
        if (!get_result)
          std::cout << "Failed to send get result to client" << "\n";

      } else {
        std::cout << "Get handler failed" << "\n";
      }
    } else if (operation == 0x30) {
      DeleteResult delete_result =
          store_controller->handleDeleteKey(key_string);
      if (delete_result.success) {
        std::string msg =
            std::to_string(operation) + std::string{"/"} + key_string;
        bool send_result = sendToClient(client_fd, msg.data());

        if (!send_result)
          std::cout << "Failed to send delete result to client" << "\n";

      } else {
        std::cout << "Delete handler failed" << "\n";
      }
    } else {
      std::cout << "Operation type not recognized" << "\n";
    }
    close(client_fd);
    continue;
  }
  close(server_fd);
  return 0;
}

bool TCPServer::sendToClient(int client_fd, const char *msg) {
  int len = std::strlen(msg);

  ssize_t total_sent = 0;
  while (total_sent < len) {
    ssize_t n = send(client_fd, msg + total_sent, len - total_sent, 0);

    if (n <= 0) {
      perror("send");
      return false;
    }

    total_sent += n;
  }

  return true;
}
