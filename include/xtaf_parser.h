#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

struct XTAFEntry {
    std::string name;
    uint32_t    cluster;
    uint32_t    size;
    bool        isDirectory;
};

struct XTAFGame {
    std::string titleId;
    std::string name;
    std::string xexPath;
    uint64_t    xexOffset;
    uint64_t    xexSize;
    bool        isGOD;
};

// main XTAF scanner — finds all installed games
std::vector<XTAFGame> scanXTAFForGames(const std::string& drivePath,
                                        uint64_t partitionOffset);

bool extractGameFiles(const std::string& drivePath,
                      const XTAFGame& game,
                      const std::string& outputDir);
