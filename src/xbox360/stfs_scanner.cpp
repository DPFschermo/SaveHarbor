#include "stfs_scanner.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

// =====================================================================
// PLATFORM HELPERS
// =====================================================================

std::string getDefaultOutputDir() {
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
    if (!home) home = "C:\\Users\\Public";
    return std::string(home) + "\\SaveHarbor_Saves";
#else
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/SaveHarbor_Saves";
#endif
}

std::string getXeniaContentDir() {
#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    if (!appdata) appdata = "C:\\Users\\Public";
    return std::string(appdata) + "\\Xenia\\content";
#else
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.config/Xenia/content";
#endif
}

// =====================================================================
// BINARY HELPERS
// =====================================================================

static uint32_t readBE32(const uint8_t* buf, size_t offset) {
    return ((uint32_t)buf[offset]   << 24) |
           ((uint32_t)buf[offset+1] << 16) |
           ((uint32_t)buf[offset+2] <<  8) |
           ((uint32_t)buf[offset+3]);
}

static std::string utf16beToString(const uint8_t* buf, size_t maxBytes) {
    std::string result;
    for (size_t i = 0; i + 1 < maxBytes; i += 2) {
        uint16_t c = ((uint16_t)buf[i] << 8) | buf[i+1];
        if (c == 0) break;
        if (c >= 32 && c < 127) result += (char)c;
    }
    return result;
}

static std::string bytesToString(const uint8_t* buf, size_t maxBytes) {
    std::string result;
    for (size_t i = 0; i < maxBytes; i++) {
        if (buf[i] == 0) break;
        if (buf[i] >= 32 && buf[i] < 127) result += (char)buf[i];
    }
    return result;
}

// =====================================================================
// STFS VALIDATION
// =====================================================================

static bool isValidSTFS(const uint8_t* data, size_t size) {
    if (size < 0x500) return false;

    bool validMagic = (memcmp(data, "CON ", 4) == 0) ||
                      (memcmp(data, "LIVE", 4) == 0) ||
                      (memcmp(data, "PIRS", 4) == 0);
    if (!validMagic) return false;

    uint32_t titleId = readBE32(data, 0x360);
    if (titleId == 0) return false;

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

    uint32_t version = readBE32(data, 0x364);
    if (version > 2) return false;

    return true;
}

// =====================================================================
// HEADER PARSING
// =====================================================================

static STFSSave parseHeader(const uint8_t* data, uint64_t offset) {
    STFSSave save;

    save.magic    = std::string((char*)data, 4);
    save.titleId  = readBE32(data, 0x360);

    auto it = KNOWN_TITLES.find(save.titleId);
    if (it != KNOWN_TITLES.end()) {
        save.titleName = it->second;
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "Unknown (0x%08X)", save.titleId);
        save.titleName = buf;
    }

    save.displayName = utf16beToString(data + 0x411, 0x80);
    save.gamertag    = bytesToString(data + 0x406, 16);
    save.date        = bytesToString(data + 0x020, 10);
    save.offset      = offset;

    return save;
}

// =====================================================================
// DRIVE SCANNER
// =====================================================================

std::vector<STFSSave> scanDriveForSaves(const std::string& drivePath) {
    std::vector<STFSSave> saves;

    std::ifstream drive(drivePath, std::ios::binary);
    if (!drive.is_open()) {
        std::cerr << "Error: cannot open " << drivePath << std::endl;
#ifdef _WIN32
        std::cerr << "Make sure you run as Administrator!" << std::endl;
#else
        std::cerr << "Try running with sudo!" << std::endl;
#endif
        return saves;
    }

    drive.seekg(0, std::ios::end);
    uint64_t driveSize = drive.tellg();
    std::cout << "Drive size: "
              << (driveSize / (1024ULL*1024*1024)) << " GB" << std::endl;

    const size_t  CHUNK    = 1024 * 1024;
    const size_t  HDR_SIZE = 0x500;
    uint64_t      offset   = 0;

    std::vector<uint8_t> chunk(CHUNK + HDR_SIZE);
    std::vector<uint8_t> header(HDR_SIZE);

    const uint8_t* MAGICS[] = {
        (uint8_t*)"CON ",
        (uint8_t*)"LIVE",
        (uint8_t*)"PIRS"
    };

    while (offset < driveSize) {
        // clean single-line progress bar
        double pct    = (double)offset / driveSize * 100.0;
        double gbDone = offset   / (1024.0*1024.0*1024.0);
        double gbTot  = driveSize / (1024.0*1024.0*1024.0);
        int    bars   = (int)(pct / 5);

        std::cout << "\r[";
        for (int i = 0; i < 20; i++)
            std::cout << (i < bars ? "#" : "-");
        std::cout << "] "
                  << std::fixed << std::setprecision(1) << pct << "% | "
                  << gbDone << "/" << gbTot << " GB | "
                  << "Found: " << saves.size()
                  << std::flush;

        drive.seekg(offset);
        drive.read((char*)chunk.data(), CHUNK + HDR_SIZE);
        size_t bytesRead = drive.gcount();
        if (bytesRead == 0) break;

        for (const uint8_t* magic : MAGICS) {
            size_t pos = 0;
            while (pos + 4 <= bytesRead) {
                // find magic manually
                size_t found = std::string::npos;
                for (size_t i = pos; i + 4 <= bytesRead; i++) {
                    if (memcmp(chunk.data() + i, magic, 4) == 0) {
                        found = i;
                        break;
                    }
                }
                if (found == std::string::npos) break;

                uint64_t absOffset = offset + found;

                drive.seekg(absOffset);
                drive.read((char*)header.data(), HDR_SIZE);

                if ((size_t)drive.gcount() == HDR_SIZE &&
                    isValidSTFS(header.data(), HDR_SIZE)) {

                    bool duplicate = false;
                    for (const auto& s : saves) {
                        if (s.offset == absOffset) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate)
                        saves.push_back(parseHeader(header.data(), absOffset));
                }
                pos = found + 1;
            }
        }

        offset += CHUNK;
    }

    // complete progress bar
    double gbTot = driveSize / (1024.0*1024.0*1024.0);
    std::cout << "\r[####################] 100.0% | "
              << std::fixed << std::setprecision(1)
              << gbTot << "/" << gbTot << " GB | "
              << "Found: " << saves.size() << std::endl;

    drive.close();
    return saves;
}

// =====================================================================
// RAW EXTRACTION
// =====================================================================

bool extractSave(const std::string& drivePath,
                 const STFSSave& save,
                 const std::string& outputDir) {

    fs::create_directories(outputDir);

    // build safe filename from title name
    std::string safeName = save.titleName;
    for (char& c : safeName)
        if (!isalnum(c) && c != '_' && c != '-') c = '_';

    char offsetBuf[32];
    snprintf(offsetBuf, sizeof(offsetBuf), "_%llx.stfs",
             (unsigned long long)save.offset);

    std::string filename   = safeName + offsetBuf;
    std::string outputPath = outputDir +
#ifdef _WIN32
        "\\" +
#else
        "/" +
#endif
        filename;

    std::cout << "\nExtracting: " << save.titleName  << std::endl;
    std::cout << "Gamertag:   " << save.gamertag     << std::endl;
    std::cout << "Date:       " << save.date          << std::endl;
    std::cout << "To:         " << outputPath         << std::endl;

    std::ifstream drive(drivePath, std::ios::binary);
    if (!drive.is_open()) {
        std::cerr << "Error: cannot open drive!" << std::endl;
        return false;
    }

    drive.seekg(save.offset);
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

// =====================================================================
// XENIA EXTRACTION
// =====================================================================

bool extractSaveForXenia(const std::string& drivePath,
                         const STFSSave& save,
                         const std::string& xeniaContentDir) {
    // Xenia save structure:
    // <xeniaContentDir>/0000000000000000/<titleId_hex>/00000001/<filename>

    char titleIdHex[16];
    snprintf(titleIdHex, sizeof(titleIdHex), "%08X", save.titleId);

    std::string profileDir = xeniaContentDir +
#ifdef _WIN32
        "\\0000000000000000\\" + titleIdHex + "\\00000001";
#else
        "/0000000000000000/" + titleIdHex + "/00000001";
#endif

    fs::create_directories(profileDir);

    // filename is just the title ID
    std::string outputPath = profileDir +
#ifdef _WIN32
        "\\" + titleIdHex;
#else
        "/" + titleIdHex;
#endif

    std::cout << "\nExtracting for Xenia: " << save.titleName << std::endl;
    std::cout << "Title ID:   0x" << titleIdHex             << std::endl;
    std::cout << "Gamertag:   " << save.gamertag             << std::endl;
    std::cout << "To:         " << outputPath                << std::endl;

    std::ifstream drive(drivePath, std::ios::binary);
    if (!drive.is_open()) {
        std::cerr << "Error: cannot open drive!" << std::endl;
        return false;
    }

    drive.seekg(save.offset);
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

    std::cout << "Done! Save placed in Xenia content folder." << std::endl;
    std::cout << "Launch the game in Xenia and your save should appear."
              << std::endl;
    return true;
}
