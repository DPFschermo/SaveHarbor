#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include "xtaf_parser.h"
#include "stfs_scanner.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <map>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

// =====================================================================
// CROSS-PLATFORM DRIVE READER (same as stfs_scanner)
// =====================================================================

class XTAFReader {
public:
#ifdef _WIN32
    HANDLE hDrive = INVALID_HANDLE_VALUE;

    bool open(const std::string& path) {
        hDrive = CreateFileA(path.c_str(),
            GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0, NULL);
        return hDrive != INVALID_HANDLE_VALUE;
    }

    bool seek(uint64_t offset) {
        LARGE_INTEGER li;
        li.QuadPart = (LONGLONG)offset;
        return SetFilePointerEx(hDrive, li, NULL, FILE_BEGIN) != 0;
    }

    size_t read(uint8_t* buf, size_t size) {
        size_t aligned = ((size + 511) / 512) * 512;
        std::vector<uint8_t> tmp(aligned, 0);
        DWORD bytesRead = 0;
        ReadFile(hDrive, tmp.data(), (DWORD)aligned, &bytesRead, NULL);
        size_t actual = (std::min)((size_t)bytesRead, size);
        memcpy(buf, tmp.data(), actual);
        return actual;
    }

    void close() {
        if (hDrive != INVALID_HANDLE_VALUE) {
            CloseHandle(hDrive);
            hDrive = INVALID_HANDLE_VALUE;
        }
    }

    bool isOpen() { return hDrive != INVALID_HANDLE_VALUE; }

#else
    std::ifstream f;

    bool open(const std::string& path) {
        f.open(path, std::ios::binary);
        return f.is_open();
    }

    bool seek(uint64_t offset) {
        f.seekg(offset);
        return f.good();
    }

    size_t read(uint8_t* buf, size_t size) {
        f.read((char*)buf, size);
        return f.gcount();
    }

    void close() { f.close(); }
    bool isOpen() { return f.is_open(); }
#endif
};

// =====================================================================
// XTAF CONSTANTS
// =====================================================================

const uint64_t XBOX360_DATA_PARTITION = 4496818176ULL;
const uint64_t XTAF_DATA_OFFSET = 0x18000;
const uint32_t XTAF_CLUSTER_SIZE = 16384;
const uint64_t XTAF_FAT_OFFSET = 0x1000;

// =====================================================================
// HELPERS
// =====================================================================

static uint32_t readBE32buf(const uint8_t* buf, size_t offset) {
    return ((uint32_t)buf[offset]   << 24) |
           ((uint32_t)buf[offset+1] << 16) |
           ((uint32_t)buf[offset+2] <<  8) |
           ((uint32_t)buf[offset+3]);
}

static uint64_t clusterToOffset(uint64_t partBase,
                                 uint32_t cluster) {
    uint64_t dataStart = partBase + XTAF_DATA_OFFSET;
    // cluster 0 = first cluster in data region
    return dataStart + (uint64_t)(cluster) * XTAF_CLUSTER_SIZE;
}

static std::vector<uint32_t> getFATChain(XTAFReader& drive,
                                          uint64_t    partBase,
                                          uint32_t    startCluster) {
    std::vector<uint32_t> chain;
    uint32_t current = startCluster;

    const int MAX_CLUSTERS = 1024 * 1024;
    int count = 0;

    while (current != 0xFFFFFFFF &&
           current != 0x00000000 &&
           count < MAX_CLUSTERS) {

        chain.push_back(current);

        uint64_t fatEntryOffset = partBase + XTAF_FAT_OFFSET +
                                  (uint64_t)current * 4;

        uint8_t buf[4] = {0};
        drive.seek(fatEntryOffset);
        drive.read(buf, 4);

        uint32_t next = readBE32buf(buf, 0);

        if (next >= 0xFFF8FFFF) break;

        current = next;
        count++;
    }

    return chain;
}

// =====================================================================
// DIRECTORY READER
// =====================================================================

static std::vector<XTAFEntry> readDirectory(XTAFReader& drive,
                                              uint64_t    partBase,
                                              uint32_t    dirCluster) {
    std::vector<XTAFEntry> entries;

    std::vector<uint32_t> chain = getFATChain(drive, partBase, dirCluster);
    if (chain.empty()) return entries;

    for (uint32_t cluster : chain) {
        uint64_t clusterOffset = clusterToOffset(partBase, cluster);
        drive.seek(clusterOffset);

        for (int i = 0; i < 256; i++) {
            uint8_t entry[64] = {0};
            size_t bytesRead = drive.read(entry, 64);
            if (bytesRead < 64) goto done;

            uint8_t nameLen = entry[0];

            if (nameLen == 0xFF) goto done;
            if (nameLen == 0x00) continue;
            if (nameLen > 42) continue;

            uint8_t flags   = entry[1];
            uint32_t cluster_n = readBE32buf(entry, 44);
            uint32_t size_n    = readBE32buf(entry, 48);

            std::string name = "";
            for (int j = 0; j < nameLen && j < 42; j++) {
                char c = (char)entry[2 + j];
                if (c >= 32 && c < 127) name += c;
            }

            if (name.empty()) continue;

            XTAFEntry e;
            e.name        = name;
            e.cluster     = cluster_n;
            e.size        = size_n;
            e.isDirectory = (flags & 0x10) != 0;
            entries.push_back(e);
        }
    }

done:
    return entries;
}

// =====================================================================
// RECURSIVE GAME FINDER
// =====================================================================

static bool isTitleId(const std::string& s) {
    if (s.length() != 8) return false;
    for (char c : s) {
        if (!isxdigit((unsigned char)c)) return false;
    }
    return true;
}

static bool isContentTypeFolder(const std::string& s) {
    if (s.length() != 8) return false;
    for (char c : s) {
        if (!isxdigit((unsigned char)c)) return false;
    }
    return true;
}

static void findGamesInProfile(XTAFReader&          drive,
                                uint64_t              partBase,
                                uint32_t              profileCluster,
                                std::vector<XTAFGame>& games) {

    std::vector<XTAFEntry> titleDirs =
        readDirectory(drive, partBase, profileCluster);

    for (const auto& titleDir : titleDirs) {
        if (!titleDir.isDirectory) continue;
        if (!isTitleId(titleDir.name)) continue;

        std::string titleId = titleDir.name;

        std::vector<XTAFEntry> contentDirs =
            readDirectory(drive, partBase, titleDir.cluster);

        for (const auto& contentDir : contentDirs) {
            if (!contentDir.isDirectory) continue;
            if (!isContentTypeFolder(contentDir.name)) continue;

            if (contentDir.name == "00000001") continue;
            if (contentDir.name == "00000002") continue;

            std::vector<XTAFEntry> contentFiles =
                readDirectory(drive, partBase, contentDir.cluster);

            for (const auto& file : contentFiles) {
                if (file.isDirectory) {
                    std::vector<XTAFEntry> inner =
                        readDirectory(drive, partBase, file.cluster);

                    for (const auto& innerFile : inner) {
                        if (innerFile.name == "default.xex" ||
                            innerFile.name == "default2.xex") {

                            XTAFGame game;
                            game.titleId  = titleId;
                            game.name     = file.name;
                            game.isGOD    = false;
                            game.xexOffset = clusterToOffset(
                                partBase, innerFile.cluster);
                            game.xexSize   = innerFile.size;

                            game.xexPath = titleId + "/" +
                                           contentDir.name + "/" +
                                           file.name + "/default.xex";
                            games.push_back(game);
                            break;
                        }
                    }
                } else {
                    if (file.name == "default.xex" ||
                        file.name == "default2.xex") {

                        XTAFGame game;
                        game.titleId   = titleId;
                        game.name      = contentDir.name;
                        game.isGOD     = false;
                        game.xexOffset = clusterToOffset(
                            partBase, file.cluster);
                        game.xexSize   = file.size;
                        game.xexPath   = titleId + "/" +
                                         contentDir.name + "/default.xex";
                        games.push_back(game);
                    }
                }
            }
        }
    }
}

// =====================================================================
// MAIN SCANNER
// =====================================================================

std::vector<XTAFGame> scanXTAFForGames(const std::string& drivePath,
                                         uint64_t partitionOffset) {
    std::vector<XTAFGame> games;

    XTAFReader drive;
    if (!drive.open(drivePath)) {
        std::cerr << "Error: cannot open drive!" << std::endl;
        return games;
    }

    uint64_t partBase = partitionOffset;
    uint64_t dataStart = partBase + XTAF_DATA_OFFSET;

    std::cout << "Walking Xbox 360 filesystem..." << std::endl;
    std::vector<XTAFEntry> rootEntries =
        readDirectory(drive, partBase, 0);

    if (rootEntries.empty()) {
        std::cout << "Root directory is empty or unreadable." << std::endl;
        drive.close();
        return games;
    }

    std::cout << "Root entries found: " << rootEntries.size() << std::endl;

    for (const auto& entry : rootEntries) {
        std::cout << "  " << (entry.isDirectory ? "[DIR]" : "[FILE]")
                  << " " << entry.name << std::endl;

        if (!entry.isDirectory) continue;

        if (entry.name == "Content" ||
            isContentTypeFolder(entry.name)) {

            findGamesInProfile(drive, partBase,
                               entry.cluster, games);
        }
    }

    drive.close();
    return games;
}

// =====================================================================
// GAME EXTRACTOR
// =====================================================================

bool extractGameFiles(const std::string& drivePath,
                      const XTAFGame&    game,
                      const std::string& outputDir) {

    if (game.xexSize == 0) {
        std::cerr << "Error: game file size is 0!" << std::endl;
        return false;
    }

    // create output directory
    fs::create_directories(outputDir);

#ifdef _WIN32
    std::string sep = "\\";
#else
    std::string sep = "/";
#endif

    std::string outputPath = outputDir + sep + "default.xex";

    std::cout << "\nExtracting game: " << game.titleId << std::endl;
    std::cout << "XEX offset:  0x" << std::hex << game.xexOffset
              << std::dec << std::endl;
    std::cout << "XEX size:    " << (game.xexSize / (1024*1024))
              << " MB" << std::endl;
    std::cout << "To:          " << outputPath << std::endl;

    XTAFReader drive;
    if (!drive.open(drivePath)) {
        std::cerr << "Error: cannot open drive!" << std::endl;
        return false;
    }

    drive.seek(game.xexOffset);

    // extract in 1MB chunks
    const size_t CHUNK = 1024 * 1024;
    uint64_t remaining = game.xexSize;
    uint64_t extracted = 0;

    std::vector<uint8_t> buf(CHUNK);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: cannot write to " << outputPath << std::endl;
        drive.close();
        return false;
    }

    while (remaining > 0) {
        size_t toRead = (size_t)(std::min)((uint64_t)CHUNK, remaining);
        size_t bytesRead = drive.read(buf.data(), toRead);
        if (bytesRead == 0) break;

        out.write((char*)buf.data(), bytesRead);
        extracted  += bytesRead;
        remaining  -= bytesRead;

        // progress
        double pct = (double)extracted / game.xexSize * 100.0;
        std::cout << "\r  Progress: "
                  << std::fixed << std::setprecision(1) << pct << "%"
                  << " (" << (extracted / (1024*1024)) << " MB)"
                  << std::flush;
    }

    std::cout << std::endl;
    out.close();
    drive.close();

    std::cout << "Done! Extracted " << (extracted / (1024*1024))
              << " MB" << std::endl;
    return extracted > 0;
}
