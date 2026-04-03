#include <iostream>
#include "stfs_parser.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: ./saveharbor <path-to-save-file>" << std::endl;
        return 1;
    }

    std::string filePath = argv[1];
    std::cout << "SaveHarbor v0.1 - reading: " << filePath << std::endl;

    STFSHeader header = parseSTFSHeader(filePath);

    return 0;
}
