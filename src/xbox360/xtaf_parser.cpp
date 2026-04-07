#include "xtaf_parser.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <iomanip>

uint32_t readBigEndian32(std::ifstream& file) {
    uint8_t bytes[4];
    file.read(reinterpret_cast<char*>(bytes), 4);
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

void scanDirectory(std::ifstream& drive, uint64_t dirOffset,
                   uint64_t dataStart, uint32_t clusterSize,
                   int depth = 0) {

    drive.seekg(dirOffset);
    std::string indent(depth * 2, ' ');

    for (int i = 0; i < 256; i++) {
        uint8_t  nameLen;
        uint8_t  flags;
        char     rawName[42] = {0};
        uint32_t cluster;
        uint32_t fileSize;

        drive.read(reinterpret_cast<char*>(&nameLen), 1);
        if (nameLen == 0xFF || nameLen == 0x00) break;
        if (nameLen > 42) { drive.seekg(63, std::ios::cur); continue; }

        drive.read(reinterpret_cast<char*>(&flags), 1);
        drive.read(rawName, 42);
        cluster  = readBigEndian32(drive);
        fileSize = readBigEndian32(drive);
        drive.seekg(8, std::ios::cur);

        std::string name = "";
        for (int j = 0; j < nameLen && j < 42; j++) {
            char c = rawName[j];
            if (c >= 32 && c < 127) name += c;
        }

        bool isDir = (flags & 0x10) != 0;
        uint64_t entryOffset = dataStart + ((uint64_t)(cluster - 1) * clusterSize);

        std::cout << indent
                  << (isDir ? "[DIR] " : "[FILE]")
                  << " " << name
                  << " (" << std::dec << fileSize << " bytes)"
                  << std::endl;

        if (isDir && depth < 4 && cluster > 0 && cluster != 0xFFFFFFFF) {
            uint64_t savedPos = drive.tellg();
            scanDirectory(drive, entryOffset, dataStart, clusterSize, depth + 1);
            drive.seekg(savedPos);
        }
    }
}

std::vector<XTAFEntry> scanXTAFPartition(const std::string& drivePath, uint64_t partitionOffset) {
    std::vector<XTAFEntry> entries;

    std::ifstream drive(drivePath, std::ios::binary);
    if (!drive.is_open()) {
        std::cout << "Error: could not open drive: " << drivePath << std::endl;
        std::cout << "Try with sudo!" << std::endl;
        return entries;
    }

    drive.seekg(partitionOffset);
    XTAFHeader header;
    drive.read(header.magic, 4);

    if (strncmp(header.magic, "XTAF", 4) != 0) {
        std::cout << "Error: no XTAF signature at offset " << partitionOffset << std::endl;
        return entries;
    }

    header.id                  = readBigEndian32(drive);
    header.sectorsPerCluster   = readBigEndian32(drive);
    header.rootDirFirstCluster = readBigEndian32(drive);

    uint32_t clusterSize = header.sectorsPerCluster * 512;

    std::cout << "=== XTAF Partition Found ===" << std::endl;
    std::cout << "Sectors per cluster:  " << header.sectorsPerCluster << std::endl;
    std::cout << "Root dir cluster:     " << header.rootDirFirstCluster << std::endl;
    std::cout << "Cluster size:         " << clusterSize << " bytes" << std::endl;

    uint64_t dataStart     = partitionOffset + 0x18000;
    uint64_t rootDirOffset = dataStart +
        ((uint64_t)(header.rootDirFirstCluster - 1) * clusterSize);

    std::cout << "Data start:           0x" << std::hex << dataStart << std::dec << std::endl;
    std::cout << "Root dir offset:      0x" << std::hex << rootDirOffset << std::dec << std::endl;

    std::cout << "\n=== Scanning filesystem ===" << std::endl;
    scanDirectory(drive, rootDirOffset, dataStart, clusterSize);

    drive.close();
    return entries;
}
