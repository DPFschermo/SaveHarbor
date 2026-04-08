#include "stfs_scanner.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <algorithm>

namespace fs = std::filesystem;

// read a big-endian uint32 from a byte buffer at given offset
static uint32_t readBE32(const uint8_t* buf, size_t offset) {
    return ((uint32_t)buf[offset]   << 24) |
           ((uint32_t)buf[offset+1] << 16) |
           ((uint32_t)buf[offset+2] <<  8) |
           ((uint32_t)buf[offset+3]);
}

// convert UTF-16 BE bytes to ASCII string
static std::string utf16beToString(const uint8_t* buf, size_t maxBytes) {
    std::string result;
    for (size_t i = 0; i + 1 < maxBytes; i += 2) {
        uint16_t c = ((uint16_t)buf[i] << 8) | buf[i+1];
        if (c == 0) break;
        if (c >= 32 && c < 127) result += (char)c;
    }
    return result;
}

// convert raw bytes to printable ASCII string
static std::string bytesToString(const uint8_t* buf, size_t maxBytes) {
    std::string result;
    for (size_t i = 0; i < maxBytes; i++) {
        if (buf[i] == 0) break;
        if (buf[i] >= 32 && buf[i] < 127) result += (char)buf[i];
    }
    return result;
}

// strictly validate an STFS header — filters out false positives
static bool isValidSTFS(const uint8_t* data, size_t size) {
    if (size < 0x500) return false;

    // check magic
    bool validMagic = (memcmp(data, "CON ", 4) == 0) ||
                      (memcmp(data, "LIVE", 4) == 0) ||
                      (memcmp(data, "PIRS", 4) == 0);
    if (!validMagic) return false;

    // title ID at 0x360 must be non-zero
    uint32_t titleId = readBE32(data, 0x360);
    if (titleId == 0) return false;

    // content type at 0x344 must be a known value
    uint32_t contentType = readBE32(data, 0x344);
    static const uint32_t validTypes[] = {
        0x00000001, 0x00000002, 0x00000003, 0x00000004,
        0x000D0000, 0x00090000, 0x00040000, 0x000B0000
    };
    bool validType = false;
    for (uint32_t t : validTypes) {
        if (contentType == t) { validType = true; break; }
    }
    if (!validType) return false;

    // version at 0x364 should be 0, 1, or 2
    uint32_t version = readBE32(data, 0x364);
    if (version > 2) return false;

    return true;
}

// parse a validated STFS header into an STFSSave struct
static STFSSave parseHeader(const uint8_t* data, uint64_t offset) {
    STFSSave save;

    // magic
    save.magic = std::string((char*)data, 4);
    save.magic = save.magic.substr(0, 3); // trim trailing space from "CON "
    if (save.magic == "CON") save.magic = "CON ";
    else save.magic = std::string((char*)data, 4);

    // title ID
    save.titleId = readBE32(data, 0x360);

    // look up title name
    auto it = KNOWN_TITLES.find(save.titleId);
    if (it != KNOWN_TITLES.end()) {
        save.titleName = it->second;
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "Unknown (0x%08X)", save.titleId);
        save.titleName = buf;
    }

    // display name at 0x411 (UTF-16 BE, up to 0x80 bytes)
    save.displayName = utf16beToString(data + 0x411, 0x80);

    // gamertag at 0x406 (ASCII, up to 16 bytes)
    save.gamertag = bytesToString(data + 0x406, 16);

    // date at 0x020 (ASCII)
    save.date = bytesToString(data + 0x020, 10);

    save.offset = offset;
    return save;
}

std::vector<STFSSave> scanDriveForSaves(const std::string& drivePath) {
    std::vector<STFSSave> saves;

    std::ifstream drive(drivePath, std::ios::binary);
    if (!drive.is_open()) {
        std::cerr << "Error: cannot open " << drivePath << std::endl;
        std::cerr << "Try running with sudo!" << std::endl;
        return saves;
    }

    // get drive size
    drive.seekg(0, std::ios::end);
    uint64_t driveSize = drive.tellg();

    std::cout << "Drive size: " << (driveSize / (1024*1024*1024)) << " GB" << std::endl;

    const size_t  CHUNK    = 1024 * 1024; // 1MB chunks
    const size_t  HDR_SIZE = 0x500;       // STFS header size
    uint64_t      offset   = 0;

    std::vector<uint8_t> chunk(CHUNK + HDR_SIZE);
    std::vector<uint8_t> header(HDR_SIZE);

    const uint8_t* MAGICS[] = {
        (uint8_t*)"CON ",
        (uint8_t*)"LIVE",
        (uint8_t*)"PIRS"
    };

    while (offset < driveSize) {
        // progress bar
        double pct     = (double)offset / driveSize * 100.0;
        double gbDone  = offset / (1024.0*1024.0*1024.0);
        double gbTotal = driveSize / (1024.0*1024.0*1024.0);
        int    bars    = (int)(pct / 5);

        std::cout << "\r[";
        for (int i = 0; i < 20; i++) std::cout << (i < bars ? "\xE2\x96\x88" : "\xE2\x96\x91");
        std::cout << "] " << std::fixed << std::setprecision(1)
                  << pct << "% | "
                  << gbDone << "/" << gbTotal << " GB | "
                  << "Found: " << saves.size()
                  << std::flush;

        // read chunk
        drive.seekg(offset);
        drive.read((char*)chunk.data(), CHUNK + HDR_SIZE);
        size_t bytesRead = drive.gcount();
        if (bytesRead == 0) break;

        // search for each magic
        for (const uint8_t* magic : MAGICS) {
            size_t pos = 0;
            while (pos + 4 <= bytesRead) {
                // find magic in chunk
                void* found = nullptr;
                for (size_t i = pos; i + 4 <= bytesRead; i++) {
                    if (memcmp(chunk.data() + i, magic, 4) == 0) {
                        found = chunk.data() + i;
                        pos = i;
                        break;
                    }
                }
                if (!found) break;

                uint64_t absOffset = offset + pos;

                // read full header from drive
                drive.seekg(absOffset);
                drive.read((char*)header.data(), HDR_SIZE);

                if ((size_t)drive.gcount() == HDR_SIZE &&
                    isValidSTFS(header.data(), HDR_SIZE)) {

                    // avoid duplicates
                    bool duplicate = false;
                    for (const auto& s : saves) {
                        if (s.offset == absOffset) { duplicate = true; break; }
                    }

                    if (!duplicate) {
                        saves.push_back(parseHeader(header.data(), absOffset));
                    }
                }

                pos++; // move past this match
            }
        }

        offset += CHUNK;
    }

    // final progress
    double gbTotal = driveSize / (1024.0*1024.0*1024.0);
    std::cout << "\r[\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88"
                 "\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88"
                 "\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88"
                 "\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88"
              << "] 100.0% | " << std::fixed << std::setprecision(1)
              << gbTotal << "/" << gbTotal << " GB | Found: " << saves.size()
              << std::endl;

    drive.close();
    return saves;
}

bool extractSave(const std::string& drivePath,
                 const STFSSave& save,
                 const std::string& outputDir) {

    // create output directory
    fs::create_directories(outputDir);

    // build safe filename
    std::string safeName = save.titleName;
    for (char& c : safeName) {
        if (!isalnum(c) && c != '_' && c != '-') c = '_';
    }

    char offsetBuf[32];
    snprintf(offsetBuf, sizeof(offsetBuf), "_%lx.stfs", (unsigned long)save.offset);
    std::string filename = safeName + offsetBuf;
    std::string outputPath = outputDir + "/" + filename;

    std::cout << "\nExtracting: " << save.titleName << std::endl;
    std::cout << "Gamertag:   " << save.gamertag    << std::endl;
    std::cout << "Date:       " << save.date         << std::endl;
    std::cout << "To:         " << outputPath        << std::endl;

    std::ifstream drive(drivePath, std::ios::binary);
    if (!drive.is_open()) {
        std::cerr << "Error: cannot open drive!" << std::endl;
        return false;
    }

    drive.seekg(save.offset);

    // extract 4MB — enough for any save
    const size_t EXTRACT_SIZE = 4 * 1024 * 1024;
    std::vector<uint8_t> data(EXTRACT_SIZE);
    drive.read((char*)data.data(), EXTRACT_SIZE);
    size_t bytesRead = drive.gcount();
    drive.close();

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: cannot write to " << outputPath << std::endl;
        return false;
    }

    out.write((char*)data.data(), bytesRead);
    out.close();

    std::cout << "Done! (" << bytesRead / 1024 << "KB extracted)" << std::endl;
    return true;
}
