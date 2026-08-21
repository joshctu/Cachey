#pragma once

#include <filesystem>
#include <stdint.h>

// 256 MB

struct LogSegment {
  unsigned long id;
  std::filesystem::path file_path;
  uint32_t size;
};
