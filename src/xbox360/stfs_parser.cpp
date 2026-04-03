// STFS Parser - Xbox 360 save container reader

#include "stfs_parser.h"
#include <iostream>
#include <fstream>
#include <cstring>

STFSHeader parseSTFSHeader(const std::string& filePath) {
  STFSHeader header;

  std::ifstream file(filePath, std::ios::binary);

  if(!file.is_open()) {
    std::cout << "Error: could not open file: " << filePath << std::endl;
    return header;
  }

  // read the first 4 bytes to check
  file.read(header.magic, 4);

  if(strncmp(header.magic, "CON ", 4) == 0) {
    std::cout << "Found CON container (Offline Save)" << std::endl;
  } else if (strncmp(header.magic, "LIVE", 4) == 0) {
    std::cout << "Found LIVE container" << std::endl;
  } else if (strncmp(header.magic, "PIRS", 4) == 0) {
    std::cout << "Found PIRS container" << std::endl;
  } else {
    std::cout << "Unknown file format - not an STFS container" << std::endl;
  }

  file.close();
  return header;
}
