#pragma once

#include <stdexcept>

class ReadException : public std::runtime_error {

public:
  explicit ReadException(const std::string &msg) : std::runtime_error(msg) {}
};

class WriteException : public std::runtime_error {

public:
  explicit WriteException(const std::string &msg) : std::runtime_error(msg) {}
};
