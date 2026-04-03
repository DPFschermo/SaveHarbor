#pragma once
#include <string>
#include <cstdint>

struct STFSHeader {
  char      magic[4];
  uint32_t  titleId;
  std::string displayName;
};

STFSHeader parseSTFSHeader(const std::string& filePath);
