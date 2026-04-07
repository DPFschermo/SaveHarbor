#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct XTAFHeader {
  char      magic[4];
  uint32_t  id;
  uint32_t  sectorsPerCluster;
  uint32_t  rootDirFirstCluster;
};

struct XTAFEntry {
  std::string name;
  uint32_t    size;
  bool        isDirectory;
  uint64_t    offset;
};

std::vector<XTAFEntry> scanXTAFPartition(const std::string& drivePath, uint64_t partitionOffset);
