#include <iostream>
#include "stfs_parser.h"
#include "xtaf_parser.h"

const uint64_t XBOX360_DATA_PARTITION = 4496818176ULL;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: sudo ./saveharbor <drive>" << std::endl;
        std::cout << "Example: sudo ./saveharbor /dev/sda" << std::endl;
        return 1;
    }

    std::string drivePath = argv[1];
    std::cout << "SaveHarbor v0.1 - scanning: " << drivePath << std::endl;
    std::cout << std::endl;

    auto entries = scanXTAFPartition(drivePath, XBOX360_DATA_PARTITION);

    std::cout << "\nFound " << entries.size() << " entries in root directory" << std::endl;
    return 0;
}
